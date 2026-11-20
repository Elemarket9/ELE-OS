/*
 * Copyright 2026 Sweet Meet
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.eleos.network;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.nio.file.StandardOpenOption;
import java.util.TreeSet;

/**
 * Per-app network choice. A package with no entry is granted, so install
 * compatibility does not need a row for every app. A denial is one fact:
 * that app should observe the network as down. There is no per-transport
 * field and no baseband control.
 *
 * <p>This class stores and interprets the choice. It does not open a socket.
 */
public final class NetworkPolicy {
    public static final String HEADER = "# ELE-OS network policy v1";

    /** What the app should observe. This does not open a socket. */
    public static final class Observe {
        public final boolean delivered;
        /** Null when delivered. {@code ENETUNREACH} when the network is down. */
        public final String errno;

        private Observe(boolean delivered, String errno) {
            this.delivered = delivered;
            this.errno = errno;
        }
    }

    public enum Access {
        GRANTED("granted"),
        NETWORK_DOWN("network-down");

        public final String reason;

        Access(String reason) {
            this.reason = reason;
        }
    }

    private final TreeSet<String> denied = new TreeSet<>();

    public static Observe outcomeFor(Access access) {
        if (access == null) {
            throw new IllegalArgumentException("missing access");
        }
        if (access == Access.GRANTED) {
            return new Observe(true, null);
        }
        return new Observe(false, "ENETUNREACH");
    }

    public static boolean isPackageName(String name) {
        if (name == null || name.isEmpty() || name.length() > 255) {
            return false;
        }
        String[] parts = name.split("\\.", -1);
        if (parts.length < 1) {
            return false;
        }
        for (int i = 0; i < parts.length; i++) {
            String part = parts[i];
            if (part.isEmpty() || !isSegment(part)) {
                return false;
            }
        }
        return true;
    }

    public static NetworkPolicy load(String text) throws PolicyFormatException {
        if (text == null) {
            throw new PolicyFormatException("missing text");
        }
        if (text.indexOf('\0') >= 0) {
            throw new PolicyFormatException("NUL in policy");
        }
        NetworkPolicy policy = new NetworkPolicy();
        String[] lines = text.split("\n", -1);
        for (int i = 0; i < lines.length; i++) {
            String line = lines[i];
            if (!line.isEmpty() && line.charAt(line.length() - 1) == '\r') {
                line = line.substring(0, line.length() - 1);
            }
            String trimmed = line.trim();
            if (trimmed.isEmpty() || trimmed.charAt(0) == '#') {
                continue;
            }
            String[] parts = trimmed.split("\\s+");
            int number = i + 1;
            if (parts.length != 2 || !"off".equals(parts[1]) || !isPackageName(parts[0])) {
                throw new PolicyFormatException("line " + number);
            }
            if (!policy.denied.add(parts[0])) {
                throw new PolicyFormatException("line " + number);
            }
        }
        return policy;
    }

    public static NetworkPolicy read(Path path) throws IOException, PolicyFormatException {
        if (path == null) {
            throw new IOException("missing path");
        }
        if (Files.notExists(path)) {
            return new NetworkPolicy();
        }
        return load(Files.readString(path, StandardCharsets.UTF_8));
    }

    public Access decide(String packageName) {
        requirePackage(packageName);
        if (denied.contains(packageName)) {
            return Access.NETWORK_DOWN;
        }
        return Access.GRANTED;
    }

    /** {@code on} removes a denial. The granted state is not written as a line. */
    public void setNetwork(String packageName, boolean on) {
        requirePackage(packageName);
        if (on) {
            denied.remove(packageName);
        } else {
            denied.add(packageName);
        }
    }

    public String save() {
        StringBuilder out = new StringBuilder();
        out.append(HEADER).append('\n');
        for (String packageName : denied) {
            out.append(packageName).append(" off\n");
        }
        return out.toString();
    }

    public void writeTo(Path path) throws IOException {
        if (path == null || path.getParent() == null || path.getFileName() == null) {
            throw new IOException("missing path");
        }
        Path dir = path.getParent();
        Files.createDirectories(dir);
        Path tmp = dir.resolve(path.getFileName().toString() + ".tmp");
        Files.writeString(tmp, save(), StandardCharsets.UTF_8,
                StandardOpenOption.CREATE, StandardOpenOption.TRUNCATE_EXISTING,
                StandardOpenOption.WRITE);
        try {
            try {
                Files.move(tmp, path, StandardCopyOption.REPLACE_EXISTING,
                        StandardCopyOption.ATOMIC_MOVE);
            } catch (AtomicMoveNotSupportedException ex) {
                Files.move(tmp, path, StandardCopyOption.REPLACE_EXISTING);
            }
        } catch (IOException ex) {
            Files.deleteIfExists(tmp);
            throw ex;
        }
    }

    private static void requirePackage(String packageName) {
        if (!isPackageName(packageName)) {
            throw new IllegalArgumentException("package name");
        }
    }

    private static boolean isSegment(String part) {
        if (!isLetter(part.charAt(0))) {
            return false;
        }
        for (int i = 1; i < part.length(); i++) {
            char c = part.charAt(i);
            if (!isLetter(c) && !isDigit(c) && c != '_') {
                return false;
            }
        }
        return true;
    }

    private static boolean isLetter(char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    private static boolean isDigit(char c) {
        return c >= '0' && c <= '9';
    }
}
