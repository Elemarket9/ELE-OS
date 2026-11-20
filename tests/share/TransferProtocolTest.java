// SPDX-License-Identifier: Apache-2.0
package org.eleos.share.protocol;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

public final class TransferProtocolTest {
    private static final TransferProtocol.Limits LIMITS =
            new TransferProtocol.Limits(8, 1024 * 1024, 4, 1024);

    public static void main(String[] args) throws Exception {
        roundTripMultipleFilesAndEmptyFile();
        roundTripEmptySession();
        rejectsTraversalAndUnsupportedVersion();
        rejectsMalformedUtf8AndOversizedFrame();
        detectsDigestMismatchAndAbortsAllStagedSinks();
        senderCancellationIsFramedAndCleansSink();
        sourceLengthMismatchIsAborted();
        commitFailureReportsPartialCommitAndCleansRemainder();
        boundedReadRequestsAndLargeStreamShape();
        System.out.println("Share protocol tests passed");
    }

    private static void roundTripMultipleFilesAndEmptyFile() throws Exception {
        byte[] first = "first payload".getBytes(StandardCharsets.UTF_8);
        byte[] second = new byte[0];
        byte[] third = new byte[41];
        for (int i = 0; i < third.length; i++) third[i] = (byte) (i * 17);
        List<TransferProtocol.FileSource> sources = Arrays.asList(
                source("one.txt", first), source("empty.bin", second), source("three.bin", third));
        ByteArrayOutputStream wire = new ByteArrayOutputStream();
        TransferProtocol.send(wire, sources, neverCancelled(), LIMITS);

        RecordingProvider destinations = new RecordingProvider();
        TransferProtocol.ReceiveResult result = TransferProtocol.receive(
                new ByteArrayInputStream(wire.toByteArray()), destinations, neverCancelled(), LIMITS);
        check(result.files.size() == 3, "file count");
        check(result.totalBytes == first.length + third.length, "total byte count");
        check(Arrays.equals(destinations.files.get(0).bytes.toByteArray(), first), "first bytes");
        check(Arrays.equals(destinations.files.get(1).bytes.toByteArray(), second), "empty bytes");
        check(Arrays.equals(destinations.files.get(2).bytes.toByteArray(), third), "third bytes");
        check(destinations.committed == 3 && destinations.aborted == 0, "all sinks committed");
    }

    private static void roundTripEmptySession() throws Exception {
        ByteArrayOutputStream wire = new ByteArrayOutputStream();
        TransferProtocol.send(wire, Collections.emptyList(), neverCancelled(), LIMITS);
        RecordingProvider provider = new RecordingProvider();
        TransferProtocol.ReceiveResult result = TransferProtocol.receive(
                new ByteArrayInputStream(wire.toByteArray()), provider, neverCancelled(), LIMITS);
        check(result.files.isEmpty() && result.totalBytes == 0, "empty transfer");
    }

    private static void rejectsTraversalAndUnsupportedVersion() throws Exception {
        expect(IllegalArgumentException.class,
                () -> new TransferProtocol.FileSource("../escape", 0,
                        () -> new ByteArrayInputStream(new byte[0])));
        ByteArrayOutputStream wire = new ByteArrayOutputStream();
        TransferProtocol.send(wire, Collections.emptyList(), neverCancelled(), LIMITS);
        byte[] version = wire.toByteArray();
        version[5] = 2;
        RecordingProvider provider = new RecordingProvider();
        expect(TransferProtocol.ProtocolException.class, () -> TransferProtocol.receive(
                new ByteArrayInputStream(version), provider, neverCancelled(), LIMITS));
        check(provider.opened == 0, "unsupported version rejected before sink creation");
    }

    private static void rejectsMalformedUtf8AndOversizedFrame() throws Exception {
        byte[] malformed = sessionWithMetadata(new byte[] {(byte) 0xc3, 0x28}, 0);
        expect(TransferProtocol.ProtocolException.class, () -> TransferProtocol.receive(
                new ByteArrayInputStream(malformed), new RecordingProvider(), neverCancelled(), LIMITS));

        byte[] valid = encode(Collections.singletonList(source("x", new byte[] {1, 2})));
        // Header (12), FILE_START header (5), then its bounded metadata payload.
        int dataFrameHeader = 12 + 5 + 11;
        valid[dataFrameHeader + 1] = 0x7f;
        valid[dataFrameHeader + 2] = 0x7f;
        valid[dataFrameHeader + 3] = 0x7f;
        valid[dataFrameHeader + 4] = 0x7f;
        RecordingProvider provider = new RecordingProvider();
        expect(TransferProtocol.ProtocolException.class, () -> TransferProtocol.receive(
                new ByteArrayInputStream(valid), provider, neverCancelled(), LIMITS));
        check(provider.aborted == 1 && provider.committed == 0, "oversized frame aborts staged sink");
    }

    private static void detectsDigestMismatchAndAbortsAllStagedSinks() throws Exception {
        byte[] wire = encode(Arrays.asList(source("a", new byte[] {1, 2, 3}),
                source("b", new byte[] {4, 5, 6})));
        int firstMetadataEnd = 12 + 5 + 11;
        // Locate the second file's first data frame by walking the wire format.
        int secondStart = firstMetadataEnd + 5 + 3 + 5 + 32;
        int secondMeta = 2 + 1 + 8;
        int secondDataPayload = secondStart + 5 + secondMeta + 5;
        wire[secondDataPayload] ^= 0x40;

        RecordingProvider provider = new RecordingProvider();
        expect(TransferProtocol.ProtocolException.class, () -> TransferProtocol.receive(
                new ByteArrayInputStream(wire), provider, neverCancelled(), LIMITS));
        check(provider.committed == 0 && provider.aborted == 2,
                "digest failure aborts every uncommitted sink");
    }

    private static void senderCancellationIsFramedAndCleansSink() throws Exception {
        final int[] reads = {0};
        final boolean[] cancelled = {false};
        TransferProtocol.FileSource source = new TransferProtocol.FileSource("cancel.bin", 20, () ->
                new InputStream() {
                    private int offset;
                    @Override public int read() {
                        if (offset == 20) return -1;
                        reads[0]++;
                        offset++;
                        if (reads[0] == 4) cancelled[0] = true;
                        return 7;
                    }
                    @Override public int read(byte[] b, int off, int len) {
                        int n = 0;
                        while (n < len && offset < 20) {
                            b[off + n++] = (byte) read();
                        }
                        return n == 0 ? -1 : n;
                    }
                });
        ByteArrayOutputStream wire = new ByteArrayOutputStream();
        expect(TransferProtocol.TransferCancelledException.class, () -> TransferProtocol.send(
                wire, Collections.singletonList(source), () -> cancelled[0], LIMITS));
        RecordingProvider provider = new RecordingProvider();
        expect(TransferProtocol.TransferAbortedException.class, () -> TransferProtocol.receive(
                new ByteArrayInputStream(wire.toByteArray()), provider, neverCancelled(), LIMITS));
        check(provider.aborted == 1 && provider.committed == 0, "cancel aborts active sink");
    }

    private static void sourceLengthMismatchIsAborted() throws Exception {
        TransferProtocol.FileSource shortSource = new TransferProtocol.FileSource(
                "short.bin", 4, () -> new ByteArrayInputStream(new byte[] {1, 2}));
        ByteArrayOutputStream wire = new ByteArrayOutputStream();
        expect(TransferProtocol.ProtocolException.class, () -> TransferProtocol.send(
                wire, Collections.singletonList(shortSource), neverCancelled(), LIMITS));
        RecordingProvider provider = new RecordingProvider();
        expect(TransferProtocol.TransferAbortedException.class, () -> TransferProtocol.receive(
                new ByteArrayInputStream(wire.toByteArray()), provider, neverCancelled(), LIMITS));
        check(provider.aborted == 1, "short source sends abort and discards staging");
    }

    private static void commitFailureReportsPartialCommitAndCleansRemainder() throws Exception {
        byte[] wire = encode(Arrays.asList(source("a", new byte[] {1}), source("b", new byte[] {2})));
        RecordingProvider provider = new RecordingProvider();
        provider.failCommitAt = 2;
        TransferProtocol.PartialCommitException failure = expect(
                TransferProtocol.PartialCommitException.class, () -> TransferProtocol.receive(
                        new ByteArrayInputStream(wire), provider, neverCancelled(), LIMITS));
        check(failure.committedFiles == 1, "partial commit count");
        check(provider.committed == 1 && provider.aborted == 1, "failed and remaining sink cleaned");
    }

    private static void boundedReadRequestsAndLargeStreamShape() throws Exception {
        final long total = 4L * 1024 * 1024;
        TransferProtocol.Limits limits = new TransferProtocol.Limits(2, total, 8192, 1024);
        InputStream generated = new InputStream() {
            private long remaining = total;
            @Override public int read() {
                if (remaining == 0) return -1;
                remaining--;
                return (int) (remaining & 0xff);
            }
            @Override public int read(byte[] b, int off, int len) {
                check(len <= 8192, "source read remains chunk-bounded");
                if (remaining == 0) return -1;
                int n = (int) Math.min((long) len, remaining);
                for (int i = 0; i < n; i++) b[off + i] = (byte) read();
                return n;
            }
        };
        ByteArrayOutputStream wire = new ByteArrayOutputStream();
        TransferProtocol.send(wire, Collections.singletonList(new TransferProtocol.FileSource(
                "generated.bin", total, () -> generated)), neverCancelled(), limits);
        CountingSinkProvider provider = new CountingSinkProvider();
        TransferProtocol.receive(new ByteArrayInputStream(wire.toByteArray()), provider,
                neverCancelled(), limits);
        check(provider.bytes == total, "large stream count");
        check(provider.largestWrite <= 8192, "destination writes remain chunk-bounded");
    }

    private static byte[] sessionWithMetadata(byte[] name, long size) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        DataOutputStream out = new DataOutputStream(bytes);
        out.writeInt(0x454c5350);
        out.writeShort(1);
        out.writeShort(0);
        out.writeInt(1);
        out.writeByte(1);
        out.writeInt(name.length + 10);
        out.writeShort(name.length);
        out.write(name);
        out.writeLong(size);
        return bytes.toByteArray();
    }

    private static TransferProtocol.FileSource source(String name, byte[] bytes) {
        return new TransferProtocol.FileSource(name, bytes.length,
                () -> new ByteArrayInputStream(bytes));
    }

    private static byte[] encode(List<TransferProtocol.FileSource> sources) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        TransferProtocol.send(bytes, sources, neverCancelled(), LIMITS);
        return bytes.toByteArray();
    }

    private static TransferProtocol.Cancellation neverCancelled() { return () -> false; }

    private static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    private static <T extends Throwable> T expect(Class<T> type, ThrowingRunnable action)
            throws Exception {
        try {
            action.run();
        } catch (Throwable actual) {
            if (type.isInstance(actual)) return type.cast(actual);
            throw new AssertionError("expected " + type.getSimpleName() + " but got " + actual, actual);
        }
        throw new AssertionError("expected " + type.getSimpleName());
    }

    @FunctionalInterface private interface ThrowingRunnable { void run() throws Exception; }

    private static class RecordingProvider implements TransferProtocol.SinkProvider {
        final List<RecordingSink> files = new ArrayList<>();
        int opened;
        int committed;
        int aborted;
        int failCommitAt = -1;

        @Override public TransferProtocol.FileSink begin(TransferProtocol.FileMetadata metadata) {
            opened++;
            RecordingSink sink = new RecordingSink(this, opened, metadata);
            files.add(sink);
            return sink;
        }
    }

    private static final class RecordingSink implements TransferProtocol.FileSink {
        final RecordingProvider provider;
        final int ordinal;
        final TransferProtocol.FileMetadata metadata;
        final ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        boolean finished;

        RecordingSink(RecordingProvider provider, int ordinal,
                      TransferProtocol.FileMetadata metadata) {
            this.provider = provider;
            this.ordinal = ordinal;
            this.metadata = metadata;
        }

        @Override public OutputStream outputStream() { return bytes; }
        @Override public void commit() throws IOException {
            if (ordinal == provider.failCommitAt) throw new IOException("injected commit failure");
            if (finished) throw new IOException("sink already finalized");
            finished = true;
            provider.committed++;
        }
        @Override public void abort() {
            if (!finished) {
                finished = true;
                provider.aborted++;
            }
        }
    }

    private static final class CountingSinkProvider implements TransferProtocol.SinkProvider {
        long bytes;
        int largestWrite;
        @Override public TransferProtocol.FileSink begin(TransferProtocol.FileMetadata metadata) {
            return new TransferProtocol.FileSink() {
                private boolean done;
                @Override public OutputStream outputStream() {
                    return new OutputStream() {
                        @Override public void write(int value) { bytes++; largestWrite = Math.max(largestWrite, 1); }
                        @Override public void write(byte[] b, int off, int len) {
                            bytes += len;
                            largestWrite = Math.max(largestWrite, len);
                        }
                    };
                }
                @Override public void commit() { done = true; }
                @Override public void abort() { done = true; }
            };
        }
    }
}
