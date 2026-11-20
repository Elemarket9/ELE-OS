// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Wake-phrase matching. This compares text. It does not hear audio. */
public final class Phrase {
    private Phrase() {}

    public static boolean acceptable(String raw) {
        if (raw == null) {
            return false;
        }
        String shown = collapse(raw);
        String key = normalize(shown);
        return !key.isEmpty() && key.length() <= 64 && shown.length() <= 64;
    }

    /** Trimmed display text with internal whitespace collapsed. */
    public static String collapse(String raw) {
        StringBuilder sb = new StringBuilder();
        boolean pendingSpace = false;
        for (int i = 0; i < raw.length(); ) {
            int cp = raw.codePointAt(i);
            i += Character.charCount(cp);
            if (Character.isWhitespace(cp)) {
                if (sb.length() > 0) {
                    pendingSpace = true;
                }
            } else {
                if (pendingSpace) {
                    sb.append(' ');
                    pendingSpace = false;
                }
                sb.appendCodePoint(cp);
            }
        }
        return sb.toString();
    }

    /** Lowercase letters and digits, with everything else as a single space. */
    public static String normalize(String raw) {
        if (raw == null) {
            return "";
        }
        StringBuilder sb = new StringBuilder();
        boolean pendingSpace = false;
        for (int i = 0; i < raw.length(); ) {
            int cp = raw.codePointAt(i);
            i += Character.charCount(cp);
            if (Character.isLetterOrDigit(cp)) {
                if (pendingSpace) {
                    sb.append(' ');
                    pendingSpace = false;
                }
                sb.appendCodePoint(Character.toLowerCase(cp));
            } else if (sb.length() > 0) {
                pendingSpace = true;
            }
        }
        return sb.toString();
    }

    public static boolean contains(String transcript, String phrase) {
        return commandStart(transcript, phrase) >= 0;
    }

    /**
     * Text after the first occurrence of the phrase, with the owner's
     * capitalization kept. Empty when the phrase is the whole transcript
     * or is absent.
     */
    public static String commandAfter(String transcript, String phrase) {
        if (transcript == null) {
            return "";
        }
        String shown = collapse(transcript);
        int commandWord = commandStart(shown, phrase);
        if (commandWord < 0) {
            return "";
        }
        int seen = 0;
        for (int index = 0; index < shown.length(); ) {
            int cp = shown.codePointAt(index);
            boolean wordStart = Character.isLetterOrDigit(cp)
                    && (index == 0 || !Character.isLetterOrDigit(shown.codePointBefore(index)));
            if (wordStart) {
                if (seen == commandWord) {
                    return shown.substring(index).trim();
                }
                seen++;
            }
            index += Character.charCount(cp);
        }
        return "";
    }

    private static int commandStart(String transcript, String phrase) {
        List<String> heard = words(transcript);
        List<String> key = words(phrase);
        if (key.isEmpty() || heard.isEmpty()) {
            return -1;
        }
        for (int i = 0; i + key.size() <= heard.size(); i++) {
            boolean match = true;
            for (int j = 0; j < key.size(); j++) {
                if (!heard.get(i + j).equals(key.get(j))) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return i + key.size();
            }
        }
        return -1;
    }

    private static List<String> words(String raw) {
        String normalized = normalize(raw);
        if (normalized.isEmpty()) {
            return Collections.emptyList();
        }
        String[] parts = normalized.split(" ");
        List<String> words = new ArrayList<String>(parts.length);
        Collections.addAll(words, parts);
        return words;
    }
}
