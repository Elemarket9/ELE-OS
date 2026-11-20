// SPDX-License-Identifier: Apache-2.0
package org.eleos.share.protocol;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

/**
 * Bounded, streaming file-transfer framing for an already-authenticated
 * transport. The caller owns TLS 1.3 setup, peer authentication, stream
 * lifetime, SAF/ContentResolver access, and any destination staging policy.
 * This class implements no encryption, key exchange, or trust decisions.
 *
 * <p>One transfer occupies the supplied streams. Files are staged by the
 * receiver and committed only after every file and the session terminator
 * validate. A sink should write to temporary storage until {@link FileSink#commit()};
 * SAF implementations may use a temporary document and replace/rename it when
 * the provider supports that operation.</p>
 */
public final class TransferProtocol {
    private static final int MAGIC = 0x454c5350; // "ELSP"
    private static final int VERSION = 1;
    private static final int RESERVED = 0;

    private static final int FILE_START = 1;
    private static final int FILE_DATA = 2;
    private static final int FILE_END = 3;
    private static final int SESSION_END = 4;
    private static final int ABORT = 5;

    private static final int SHA256_BYTES = 32;
    private static final int MAX_FILES_HARD = 4096;
    private static final int MAX_CHUNK_HARD = 1024 * 1024;
    private static final int MAX_METADATA_HARD = 64 * 1024;
    private static final int MAX_NAME_BYTES = 255;
    private static final int DEFAULT_CHUNK_BYTES = 64 * 1024;
    private static final int DEFAULT_METADATA_BYTES = 4096;

    private TransferProtocol() {}

    /** Supplies one source stream, such as ContentResolver.openInputStream(uri). */
    @FunctionalInterface
    public interface InputSupplier {
        InputStream open() throws IOException;
    }

    /** A caller-owned cancellation signal. It must be quick and thread-safe. */
    @FunctionalInterface
    public interface Cancellation {
        boolean isCancelled();
    }

    /** Opens a staging destination for one validated file. */
    @FunctionalInterface
    public interface SinkProvider {
        FileSink begin(FileMetadata metadata) throws IOException;
    }

    /**
     * Destination lifecycle. The protocol closes {@link #outputStream()} before
     * commit or abort. abort must be idempotent and discard uncommitted bytes.
     */
    public interface FileSink {
        OutputStream outputStream() throws IOException;
        void commit() throws IOException;
        void abort() throws IOException;
    }

    /** Strict resource ceilings; the protocol additionally enforces hard caps. */
    public static final class Limits {
        public static final Limits DEFAULT = new Limits(
                128, Long.MAX_VALUE, DEFAULT_CHUNK_BYTES, DEFAULT_METADATA_BYTES);

        public final int maxFiles;
        public final long maxFileBytes;
        public final int maxChunkBytes;
        public final int maxMetadataBytes;

        public Limits(int maxFiles, long maxFileBytes, int maxChunkBytes,
                      int maxMetadataBytes) {
            if (maxFiles < 1 || maxFiles > MAX_FILES_HARD) {
                throw new IllegalArgumentException("maxFiles outside supported range");
            }
            if (maxFileBytes < 0) {
                throw new IllegalArgumentException("maxFileBytes must be nonnegative");
            }
            if (maxChunkBytes < 1 || maxChunkBytes > MAX_CHUNK_HARD) {
                throw new IllegalArgumentException("maxChunkBytes outside supported range");
            }
            if (maxMetadataBytes < 10 || maxMetadataBytes > MAX_METADATA_HARD) {
                throw new IllegalArgumentException("maxMetadataBytes outside supported range");
            }
            this.maxFiles = maxFiles;
            this.maxFileBytes = maxFileBytes;
            this.maxChunkBytes = maxChunkBytes;
            this.maxMetadataBytes = maxMetadataBytes;
        }
    }

    /** A declared source; size must exactly match the opened stream. */
    public static final class FileSource {
        public final String name;
        public final long size;
        private final InputSupplier inputSupplier;

        public FileSource(String name, long size, InputSupplier inputSupplier) {
            validateName(name);
            if (size < 0) throw new IllegalArgumentException("negative file size");
            this.name = name;
            this.size = size;
            this.inputSupplier = Objects.requireNonNull(inputSupplier, "inputSupplier");
        }
    }

    /** Wire metadata. Names are display names only, never paths. */
    public static final class FileMetadata {
        public final String name;
        public final long size;

        private FileMetadata(String name, long size) {
            this.name = name;
            this.size = size;
        }
    }

    /** Summary returned only after the receive-side commit phase succeeds. */
    public static final class ReceiveResult {
        public final List<FileMetadata> files;
        public final long totalBytes;

        private ReceiveResult(List<FileMetadata> files, long totalBytes) {
            this.files = Collections.unmodifiableList(new ArrayList<>(files));
            this.totalBytes = totalBytes;
        }
    }

    public enum AbortReason {
        CANCELLED(1), SOURCE_FAILURE(2), SIZE_MISMATCH(3),
        PROTOCOL_FAILURE(4), STORAGE_FAILURE(5);

        private final int wireCode;
        AbortReason(int wireCode) { this.wireCode = wireCode; }

        private static AbortReason fromWire(int code) throws ProtocolException {
            for (AbortReason reason : values()) {
                if (reason.wireCode == code) return reason;
            }
            throw new ProtocolException("unknown abort reason");
        }
    }

    public static class ProtocolException extends IOException {
        private static final long serialVersionUID = 1L;
        public ProtocolException(String message) { super(message); }
        public ProtocolException(String message, Throwable cause) { super(message, cause); }
    }

    public static final class TransferAbortedException extends IOException {
        private static final long serialVersionUID = 1L;
        public final AbortReason reason;
        private TransferAbortedException(AbortReason reason) {
            super("peer aborted transfer: " + reason);
            this.reason = reason;
        }
    }

    public static final class TransferCancelledException extends IOException {
        private static final long serialVersionUID = 1L;
        private TransferCancelledException() { super("transfer cancelled"); }
    }

    /** A sink commit failed after some earlier staged files were committed. */
    public static final class PartialCommitException extends IOException {
        private static final long serialVersionUID = 1L;
        public final int committedFiles;
        private PartialCommitException(int committedFiles, Throwable cause) {
            super("destination commit failed after " + committedFiles + " file(s)", cause);
            this.committedFiles = committedFiles;
        }
    }

    /** Streams all files in bounded chunks and emits a SHA-256 trailer per file. */
    public static void send(OutputStream output, List<FileSource> sources,
                            Cancellation cancellation, Limits limits) throws IOException {
        Objects.requireNonNull(output, "output");
        Objects.requireNonNull(sources, "sources");
        Objects.requireNonNull(cancellation, "cancellation");
        Objects.requireNonNull(limits, "limits");
        List<FileSource> files = new ArrayList<>(sources);
        if (files.size() > limits.maxFiles) throw new ProtocolException("too many files");
        long declaredTotal = 0;
        for (FileSource source : files) {
            if (source == null) throw new ProtocolException("null file source");
            validateNameForProtocol(source.name, limits);
            if (source.size > limits.maxFileBytes) throw new ProtocolException("file exceeds limit");
            if (Long.MAX_VALUE - declaredTotal < source.size) {
                throw new ProtocolException("total size overflow");
            }
            declaredTotal += source.size;
        }
        checkCancelled(cancellation);

        DataOutputStream out = new DataOutputStream(output);
        boolean headerWritten = false;
        try {
            out.writeInt(MAGIC);
            out.writeShort(VERSION);
            out.writeShort(RESERVED);
            out.writeInt(files.size());
            headerWritten = true;

            byte[] buffer = new byte[limits.maxChunkBytes];
            for (FileSource source : files) {
                checkCancelled(cancellation);
                byte[] metadata = encodeMetadata(source.name, source.size, limits);
                writeFrameHeader(out, FILE_START, metadata.length);
                out.write(metadata);

                MessageDigest digest = sha256();
                try (InputStream in = source.inputSupplier.open()) {
                    if (in == null) throw new IOException("source returned null stream");
                    long remaining = source.size;
                    while (remaining > 0) {
                        checkCancelled(cancellation);
                        int wanted = (int) Math.min((long) buffer.length, remaining);
                        int count = in.read(buffer, 0, wanted);
                        if (count < 0) {
                            throw new SourceLengthException("source shorter than declared size");
                        }
                        if (count == 0) {
                            int one = in.read();
                            if (one < 0) {
                                throw new SourceLengthException("source shorter than declared size");
                            }
                            buffer[0] = (byte) one;
                            count = 1;
                        }
                        writeFrameHeader(out, FILE_DATA, count);
                        out.write(buffer, 0, count);
                        digest.update(buffer, 0, count);
                        remaining -= count;
                    }
                    // Detect stale or incorrect size metadata rather than silently
                    // transferring only a prefix of a source that has grown.
                    if (in.read() != -1) {
                        throw new SourceLengthException("source longer than declared size");
                    }
                }
                writeFrameHeader(out, FILE_END, SHA256_BYTES);
                out.write(digest.digest());
            }
            checkCancelled(cancellation);
            writeFrameHeader(out, SESSION_END, 0);
            out.flush();
        } catch (TransferCancelledException cancelled) {
            if (headerWritten) sendAbortSafely(out, AbortReason.CANCELLED, cancelled);
            throw cancelled;
        } catch (IOException failure) {
            if (headerWritten) {
                AbortReason reason = failure instanceof SourceLengthException
                        ? AbortReason.SIZE_MISMATCH : AbortReason.SOURCE_FAILURE;
                sendAbortSafely(out, reason, failure);
            }
            throw failure;
        } catch (RuntimeException failure) {
            IOException wrapped = new IOException("source or cancellation callback failed", failure);
            if (headerWritten) sendAbortSafely(out, AbortReason.SOURCE_FAILURE, wrapped);
            throw wrapped;
        }
    }

    /** Receives, hashes, and stages a bounded multi-file transfer. */
    public static ReceiveResult receive(InputStream input, SinkProvider sinkProvider,
                                        Cancellation cancellation, Limits limits)
            throws IOException {
        Objects.requireNonNull(input, "input");
        Objects.requireNonNull(sinkProvider, "sinkProvider");
        Objects.requireNonNull(cancellation, "cancellation");
        Objects.requireNonNull(limits, "limits");

        DataInputStream in = new DataInputStream(input);
        List<PendingSink> pending = new ArrayList<>();
        List<FileMetadata> metadataList = new ArrayList<>();
        long totalBytes = 0;
        try {
            if (in.readInt() != MAGIC) throw new ProtocolException("invalid protocol magic");
            int version = in.readUnsignedShort();
            if (version != VERSION) throw new ProtocolException("unsupported protocol version: " + version);
            if (in.readUnsignedShort() != RESERVED) throw new ProtocolException("reserved header bits set");
            int fileCount = in.readInt();
            if (fileCount < 0 || fileCount > limits.maxFiles) throw new ProtocolException("file count exceeds limit");

            byte[] buffer = new byte[limits.maxChunkBytes];
            for (int i = 0; i < fileCount; i++) {
                checkCancelled(cancellation);
                Frame start = readFrameHeader(in, limits);
                rejectAbort(start, in);
                if (start.type != FILE_START) throw new ProtocolException("expected file metadata frame");
                byte[] encoded = new byte[start.length];
                in.readFully(encoded);
                FileMetadata metadata = decodeMetadata(encoded, limits);
                if (metadata.size > limits.maxFileBytes) throw new ProtocolException("file exceeds limit");
                if (Long.MAX_VALUE - totalBytes < metadata.size) throw new ProtocolException("total size overflow");
                totalBytes += metadata.size;

                FileSink sink = sinkProvider.begin(metadata);
                if (sink == null) throw new IOException("sink provider returned null");
                OutputStream destination;
                try {
                    destination = sink.outputStream();
                    if (destination == null) throw new IOException("sink returned null output stream");
                } catch (IOException | RuntimeException e) {
                    PendingSink failed = new PendingSink(metadata, sink, null);
                    abortOne(failed, e);
                    throw e;
                }
                PendingSink staged = new PendingSink(metadata, sink, destination);
                pending.add(staged);
                metadataList.add(metadata);

                MessageDigest digest = sha256();
                long remaining = metadata.size;
                while (remaining > 0) {
                    checkCancelled(cancellation);
                    Frame data = readFrameHeader(in, limits);
                    rejectAbort(data, in);
                    if (data.type != FILE_DATA) throw new ProtocolException("expected file data frame");
                    if (data.length <= 0 || data.length > remaining) {
                        throw new ProtocolException("invalid file data frame length");
                    }
                    in.readFully(buffer, 0, data.length);
                    destination.write(buffer, 0, data.length);
                    digest.update(buffer, 0, data.length);
                    remaining -= data.length;
                }

                Frame end = readFrameHeader(in, limits);
                rejectAbort(end, in);
                if (end.type != FILE_END || end.length != SHA256_BYTES) {
                    throw new ProtocolException("invalid file digest frame");
                }
                byte[] expected = new byte[SHA256_BYTES];
                in.readFully(expected);
                if (!MessageDigest.isEqual(expected, digest.digest())) {
                    throw new ProtocolException("file SHA-256 mismatch");
                }
                destination.close();
                staged.output = null;
            }

            checkCancelled(cancellation);
            Frame done = readFrameHeader(in, limits);
            rejectAbort(done, in);
            if (done.type != SESSION_END || done.length != 0) {
                throw new ProtocolException("invalid session terminator");
            }

            commitAll(pending, cancellation);
            return new ReceiveResult(metadataList, totalBytes);
        } catch (IOException failure) {
            abortAll(pending, failure);
            throw failure;
        } catch (RuntimeException failure) {
            IOException wrapped = new IOException("receive failed", failure);
            abortAll(pending, wrapped);
            throw wrapped;
        }
    }

    private static void commitAll(List<PendingSink> pending, Cancellation cancellation)
            throws IOException {
        int committed = 0;
        for (int i = 0; i < pending.size(); i++) {
            PendingSink item = pending.get(i);
            try {
                checkCancelled(cancellation);
            } catch (TransferCancelledException cancelled) {
                abortFrom(pending, i, cancelled);
                if (committed == 0) throw cancelled;
                throw new PartialCommitException(committed, cancelled);
            }
            try {
                item.sink.commit();
                item.committed = true;
                committed++;
            } catch (IOException | RuntimeException failure) {
                IOException cause = failure instanceof IOException
                        ? (IOException) failure : new IOException("sink commit failed", failure);
                abortFrom(pending, i, cause);
                abortFrom(pending, i + 1, cause);
                throw new PartialCommitException(committed, cause);
            }
        }
    }

    private static void abortAll(List<PendingSink> pending, Throwable primary) {
        abortFrom(pending, 0, primary);
    }

    private static void abortFrom(List<PendingSink> pending, int start, Throwable primary) {
        for (int i = start; i < pending.size(); i++) abortOne(pending.get(i), primary);
    }

    private static void abortOne(PendingSink item, Throwable primary) {
        if (item.committed || item.aborted) return;
        item.aborted = true;
        if (item.output != null) {
            try {
                item.output.close();
            } catch (IOException closeFailure) {
                primary.addSuppressed(closeFailure);
            }
            item.output = null;
        }
        try {
            item.sink.abort();
        } catch (IOException | RuntimeException abortFailure) {
            primary.addSuppressed(abortFailure);
        }
    }

    private static void writeAbort(DataOutputStream out, AbortReason reason) throws IOException {
        writeFrameHeader(out, ABORT, 1);
        out.writeByte(reason.wireCode);
        out.flush();
    }

    private static void sendAbortSafely(DataOutputStream out, AbortReason reason,
                                        IOException primary) {
        try {
            writeAbort(out, reason);
        } catch (IOException abortFailure) {
            primary.addSuppressed(abortFailure);
        }
    }

    private static void checkCancelled(Cancellation cancellation)
            throws TransferCancelledException {
        if (cancellation.isCancelled()) throw new TransferCancelledException();
    }

    private static void writeFrameHeader(DataOutputStream out, int type, int length)
            throws IOException {
        out.writeByte(type);
        out.writeInt(length);
    }

    private static Frame readFrameHeader(DataInputStream in, Limits limits) throws IOException {
        int type = in.readUnsignedByte();
        int length = in.readInt();
        if (length < 0) throw new ProtocolException("negative frame length");
        switch (type) {
            case FILE_START:
                if (length > limits.maxMetadataBytes) throw new ProtocolException("metadata frame exceeds limit");
                break;
            case FILE_DATA:
                if (length > limits.maxChunkBytes) throw new ProtocolException("data frame exceeds limit");
                break;
            case FILE_END:
                if (length != SHA256_BYTES) throw new ProtocolException("invalid digest frame size");
                break;
            case SESSION_END:
                if (length != 0) throw new ProtocolException("invalid terminator frame size");
                break;
            case ABORT:
                if (length != 1) throw new ProtocolException("invalid abort frame size");
                break;
            default:
                throw new ProtocolException("unknown frame type");
        }
        return new Frame(type, length);
    }

    private static void rejectAbort(Frame frame, DataInputStream in) throws IOException {
        if (frame.type == ABORT) {
            throw new TransferAbortedException(AbortReason.fromWire(in.readUnsignedByte()));
        }
    }

    private static byte[] encodeMetadata(String name, long size, Limits limits) throws IOException {
        byte[] nameBytes = encodeName(name);
        if (nameBytes.length > MAX_NAME_BYTES) throw new ProtocolException("filename exceeds limit");
        ByteArrayOutputStream bytes = new ByteArrayOutputStream(2 + nameBytes.length + 8);
        DataOutputStream metadata = new DataOutputStream(bytes);
        metadata.writeShort(nameBytes.length);
        metadata.write(nameBytes);
        metadata.writeLong(size);
        metadata.flush();
        byte[] result = bytes.toByteArray();
        if (result.length > limits.maxMetadataBytes) throw new ProtocolException("metadata exceeds limit");
        return result;
    }

    private static FileMetadata decodeMetadata(byte[] bytes, Limits limits) throws IOException {
        if (bytes.length < 10 || bytes.length > limits.maxMetadataBytes) {
            throw new ProtocolException("invalid metadata length");
        }
        DataInputStream metadata = new DataInputStream(new ByteArrayInputStream(bytes));
        int nameLength = metadata.readUnsignedShort();
        if (nameLength < 1 || nameLength > MAX_NAME_BYTES || nameLength != bytes.length - 10) {
            throw new ProtocolException("invalid filename length");
        }
        byte[] nameBytes = new byte[nameLength];
        metadata.readFully(nameBytes);
        String name = decodeName(nameBytes);
        validateNameForProtocol(name, limits);
        long size = metadata.readLong();
        if (size < 0 || metadata.available() != 0) throw new ProtocolException("invalid file size metadata");
        return new FileMetadata(name, size);
    }

    private static byte[] encodeName(String name) throws ProtocolException {
        try {
            ByteBuffer encoded = StandardCharsets.UTF_8.newEncoder()
                    .onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT)
                    .encode(CharBuffer.wrap(name));
            byte[] bytes = new byte[encoded.remaining()];
            encoded.get(bytes);
            return bytes;
        } catch (CharacterCodingException e) {
            throw new ProtocolException("filename is not valid Unicode", e);
        }
    }

    private static String decodeName(byte[] bytes) throws ProtocolException {
        try {
            return StandardCharsets.UTF_8.newDecoder()
                    .onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT)
                    .decode(ByteBuffer.wrap(bytes)).toString();
        } catch (CharacterCodingException e) {
            throw new ProtocolException("filename is not valid UTF-8", e);
        }
    }

    private static void validateNameForProtocol(String name, Limits limits) throws IOException {
        validateName(name);
        if (encodeName(name).length > MAX_NAME_BYTES) throw new ProtocolException("filename exceeds limit");
        if (10 + encodeName(name).length > limits.maxMetadataBytes) {
            throw new ProtocolException("metadata exceeds limit");
        }
    }

    private static void validateName(String name) {
        Objects.requireNonNull(name, "name");
        if (name.isEmpty() || name.equals(".") || name.equals("..")) {
            throw new IllegalArgumentException("invalid display filename");
        }
        for (int i = 0; i < name.length(); i++) {
            char c = name.charAt(i);
            if (c == '/' || c == '\\' || Character.isISOControl(c)) {
                throw new IllegalArgumentException("filename must be a single safe display name");
            }
        }
    }

    private static MessageDigest sha256() throws ProtocolException {
        try {
            return MessageDigest.getInstance("SHA-256");
        } catch (NoSuchAlgorithmException impossible) {
            throw new ProtocolException("SHA-256 unavailable", impossible);
        }
    }

    private static final class Frame {
        final int type;
        final int length;
        Frame(int type, int length) { this.type = type; this.length = length; }
    }

    private static final class SourceLengthException extends ProtocolException {
        private static final long serialVersionUID = 1L;
        SourceLengthException(String message) { super(message); }
    }

    private static final class PendingSink {
        final FileMetadata metadata;
        final FileSink sink;
        OutputStream output;
        boolean committed;
        boolean aborted;
        PendingSink(FileMetadata metadata, FileSink sink, OutputStream output) {
            this.metadata = metadata;
            this.sink = sink;
            this.output = output;
        }
    }
}
