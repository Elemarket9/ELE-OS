// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/** Recognizer used by the prototype image. It does not transcribe. */
public final class UnavailableSpeechToText implements SpeechToText {
    @Override
    public String transcribe(byte[] pcmLe16, int sampleRateHz) {
        return null;
    }
}
