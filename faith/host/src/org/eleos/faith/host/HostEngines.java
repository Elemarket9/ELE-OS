// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith.host;

import org.eleos.faith.SpeechToText;
import org.eleos.faith.TextToSpeech;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Host adapters for the open-source engines named in the architecture doc.
 * They are not compiled into EleFaithVoice. Piper is not invoked.
 */
public final class HostEngines {
    private HostEngines() {}

    /** SVOX Pico via the pico2wave program. Apache-2.0. Not a neural voice. */
    public static final class PicoCommand implements TextToSpeech {
        public static final int SAMPLE_RATE_HZ = 16000;
        public static final String LANGUAGE = "en-US";
        public static final String LINGWARE = "en-US_lh0_sg.bin";

        private final Path binary;
        private String lastError = "";

        public PicoCommand(Path binary) {
            this.binary = binary;
        }

        public String lastError() {
            return lastError;
        }

        @Override
        public byte[] synthesize(String text) {
            lastError = "";
            if (text == null || text.isBlank() || text.length() > 400 || text.indexOf('\0') >= 0 || text.startsWith("-")) {
                lastError = "refused text";
                return null;
            }
            Path wav = null;
            try {
                wav = Files.createTempFile("faith-pico-", ".wav");
                // pico2wave writes a different, longer file when the path already
                // exists, including an empty placeholder. Remove it so pico creates it.
                Files.delete(wav);
                List<String> argv = Arrays.asList(
                        binary.toString(), "-l", LANGUAGE, "-w", wav.toString(), text);
                Command.Output output = Command.run(argv, 20);
                if (output.code != 0) {
                    lastError = clip(output.stderr);
                    return null;
                }
                byte[] encoded = Files.readAllBytes(wav);
                Wav.Pcm pcm = Wav.read(encoded);
                if (pcm.channels != 1 || pcm.bitsPerSample != 16 || pcm.sampleRateHz != SAMPLE_RATE_HZ) {
                    lastError = "unexpected wav layout";
                    return null;
                }
                return pcm.samples;
            } catch (Exception ex) {
                lastError = ex.getMessage() == null ? ex.getClass().getSimpleName() : ex.getMessage();
                return null;
            } finally {
                delete(wav);
            }
        }
    }

    /**
     * whisper.cpp command-line recognizer. MIT. Weights are not in this repository.
     * The transcript is whatever the process prints. This class does not correct it.
     */
    public static final class WhisperCommand implements SpeechToText {
        private final Path binary;
        private final Path model;
        private String lastRaw = "";
        private String lastError = "";

        public WhisperCommand(Path binary, Path model) {
            this.binary = binary;
            this.model = model;
        }

        public String lastRaw() {
            return lastRaw;
        }

        public String lastError() {
            return lastError;
        }

        @Override
        public String transcribe(byte[] pcmLe16, int sampleRateHz) {
            lastRaw = "";
            lastError = "";
            if (pcmLe16 == null || pcmLe16.length < 2 || sampleRateHz <= 0) {
                lastError = "no audio";
                return null;
            }
            Path wav = null;
            try {
                wav = Files.createTempFile("faith-whisper-", ".wav");
                Files.write(wav, Wav.wrap(pcmLe16, sampleRateHz));
                List<String> argv = Arrays.asList(
                        binary.toString(),
                        "--no-gpu",
                        "--no-timestamps",
                        "--no-prints",
                        "--language", "en",
                        "-m", model.toString(),
                        "-f", wav.toString());
                Command.Output output = Command.run(argv, 120);
                lastRaw = output.stdout;
                if (output.code != 0) {
                    lastError = clip(output.stderr);
                    return null;
                }
                String transcript = output.stdout.trim().replaceAll("\\s+", " ");
                if (transcript.isEmpty()) {
                    lastError = clip(output.stderr);
                    return null;
                }
                return transcript;
            } catch (Exception ex) {
                lastError = ex.getMessage() == null ? ex.getClass().getSimpleName() : ex.getMessage();
                return null;
            } finally {
                delete(wav);
            }
        }
    }

    public static final class Wav {
        private Wav() {}

        public static final class Pcm {
            public final byte[] samples;
            public final int sampleRateHz;
            public final int channels;
            public final int bitsPerSample;

            Pcm(byte[] samples, int sampleRateHz, int channels, int bitsPerSample) {
                this.samples = samples;
                this.sampleRateHz = sampleRateHz;
                this.channels = channels;
                this.bitsPerSample = bitsPerSample;
            }
        }

        public static byte[] wrap(byte[] pcm, int sampleRateHz) {
            int channels = 1;
            int bits = 16;
            int block = channels * bits / 8;
            int data = pcm.length;
            byte[] wav = new byte[44 + data];
            writeAscii(wav, 0, "RIFF");
            write32(wav, 4, 36 + data);
            writeAscii(wav, 8, "WAVE");
            writeAscii(wav, 12, "fmt ");
            write32(wav, 16, 16);
            write16(wav, 20, 1);
            write16(wav, 22, channels);
            write32(wav, 24, sampleRateHz);
            write32(wav, 28, sampleRateHz * block);
            write16(wav, 32, block);
            write16(wav, 34, bits);
            writeAscii(wav, 36, "data");
            write32(wav, 40, data);
            System.arraycopy(pcm, 0, wav, 44, data);
            return wav;
        }

        public static Pcm read(byte[] wav) throws IOException {
            if (wav.length < 44 || !ascii(wav, 0, "RIFF") || !ascii(wav, 8, "WAVE")) {
                throw new IOException("not a wav");
            }
            int channels = 0;
            int rate = 0;
            int bits = 0;
            byte[] samples = null;
            int offset = 12;
            while (offset + 8 <= wav.length) {
                String id = new String(wav, offset, 4, StandardCharsets.US_ASCII);
                int size = read32(wav, offset + 4);
                int body = offset + 8;
                if (size < 0 || body + size > wav.length) {
                    throw new IOException("truncated wav");
                }
                if ("fmt ".equals(id) && size >= 16) {
                    int format = read16(wav, body);
                    if (format != 1) {
                        throw new IOException("wav format " + format);
                    }
                    channels = read16(wav, body + 2);
                    rate = read32(wav, body + 4);
                    bits = read16(wav, body + 14);
                } else if ("data".equals(id)) {
                    samples = Arrays.copyOfRange(wav, body, body + size);
                }
                offset = body + size + (size & 1);
            }
            if (samples == null || channels == 0 || rate == 0 || bits == 0) {
                throw new IOException("incomplete wav");
            }
            return new Pcm(samples, rate, channels, bits);
        }

        public static int rms(byte[] pcmLe16) {
            if (pcmLe16 == null || pcmLe16.length < 2) {
                return 0;
            }
            long sum = 0;
            int count = 0;
            for (int i = 0; i + 1 < pcmLe16.length; i += 2) {
                int lo = pcmLe16[i] & 0xff;
                int hi = pcmLe16[i + 1];
                int sample = (short) (lo | (hi << 8));
                sum += (long) sample * sample;
                count++;
            }
            if (count == 0) {
                return 0;
            }
            return (int) Math.sqrt(sum / count);
        }

        private static void writeAscii(byte[] dest, int offset, String value) {
            byte[] raw = value.getBytes(StandardCharsets.US_ASCII);
            System.arraycopy(raw, 0, dest, offset, raw.length);
        }

        private static void write16(byte[] dest, int offset, int value) {
            dest[offset] = (byte) value;
            dest[offset + 1] = (byte) (value >> 8);
        }

        private static void write32(byte[] dest, int offset, int value) {
            dest[offset] = (byte) value;
            dest[offset + 1] = (byte) (value >> 8);
            dest[offset + 2] = (byte) (value >> 16);
            dest[offset + 3] = (byte) (value >> 24);
        }

        private static int read16(byte[] src, int offset) {
            return (src[offset] & 0xff) | ((src[offset + 1] & 0xff) << 8);
        }

        private static int read32(byte[] src, int offset) {
            return (src[offset] & 0xff)
                    | ((src[offset + 1] & 0xff) << 8)
                    | ((src[offset + 2] & 0xff) << 16)
                    | ((src[offset + 3] & 0xff) << 24);
        }

        private static boolean ascii(byte[] src, int offset, String value) {
            byte[] raw = value.getBytes(StandardCharsets.US_ASCII);
            if (offset + raw.length > src.length) {
                return false;
            }
            for (int i = 0; i < raw.length; i++) {
                if (src[offset + i] != raw[i]) {
                    return false;
                }
            }
            return true;
        }
    }

    static final class Command {
        static final class Output {
            final int code;
            final String stdout;
            final String stderr;

            Output(int code, String stdout, String stderr) {
                this.code = code;
                this.stdout = stdout;
                this.stderr = stderr;
            }
        }

        static Output run(List<String> argv, long timeoutSeconds) throws IOException, InterruptedException {
            Process process = new ProcessBuilder(new ArrayList<String>(argv)).start();
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            ByteArrayOutputStream err = new ByteArrayOutputStream();
            Thread stdout = drain(process.getInputStream(), out);
            Thread stderr = drain(process.getErrorStream(), err);
            boolean finished = process.waitFor(timeoutSeconds, TimeUnit.SECONDS);
            if (!finished) {
                process.destroyForcibly();
                stdout.join(1000);
                stderr.join(1000);
                throw new IOException("timed out");
            }
            stdout.join(5000);
            stderr.join(5000);
            return new Output(process.exitValue(),
                    out.toString(StandardCharsets.UTF_8),
                    err.toString(StandardCharsets.UTF_8));
        }

        private static Thread drain(InputStream stream, ByteArrayOutputStream dest) {
            Thread thread = new Thread(() -> {
                byte[] buf = new byte[4096];
                try {
                    int n;
                    while ((n = stream.read(buf)) >= 0 && dest.size() < 65536) {
                        dest.write(buf, 0, n);
                    }
                } catch (IOException ignored) {
                    // The process closed the pipe.
                }
            });
            thread.setDaemon(true);
            thread.start();
            return thread;
        }
    }

    private static void delete(Path path) {
        if (path == null) {
            return;
        }
        try {
            Files.deleteIfExists(path);
        } catch (IOException ignored) {
            // A leftover temp file is not a speech result.
        }
    }

    private static String clip(String text) {
        if (text == null) {
            return "";
        }
        String trimmed = text.trim();
        return trimmed.length() <= 400 ? trimmed : trimmed.substring(0, 400);
    }
}
