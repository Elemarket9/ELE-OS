// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Locale;

/**
 * Owner-supplied cloud speech endpoint. The image ships no named provider.
 * Google speech hosts are refused. This check does not open a connection.
 */
public final class CloudEndpoint {
    public enum Decision {
        ACCEPTED, CLEARED, INVALID, REFUSED
    }

    private CloudEndpoint() {}

    public static Decision decide(String raw) {
        if (raw == null || raw.isBlank()) {
            return Decision.CLEARED;
        }
        URI uri;
        try {
            uri = new URI(raw.trim());
        } catch (URISyntaxException ex) {
            return Decision.INVALID;
        }
        String scheme = uri.getScheme();
        if (scheme == null) {
            return Decision.INVALID;
        }
        if (!scheme.equalsIgnoreCase("https") && !scheme.equalsIgnoreCase("http")) {
            return Decision.INVALID;
        }
        String host = uri.getHost();
        if (host == null || host.isBlank()) {
            return Decision.INVALID;
        }
        if (isGoogleHost(host)) {
            return Decision.REFUSED;
        }
        return Decision.ACCEPTED;
    }

    static boolean isGoogleHost(String host) {
        String normalized = host.toLowerCase(Locale.ROOT);
        while (normalized.endsWith(".")) {
            normalized = normalized.substring(0, normalized.length() - 1);
        }
        return domain(normalized, "google.com")
                || domain(normalized, "googleapis.com")
                || domain(normalized, "gstatic.com")
                || domain(normalized, "googleusercontent.com");
    }

    private static boolean domain(String host, String name) {
        return host.equals(name) || host.endsWith("." + name);
    }
}
