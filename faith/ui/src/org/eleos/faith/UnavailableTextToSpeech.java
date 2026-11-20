// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/** Synthesizer used by the prototype image. It does not speak. */
public final class UnavailableTextToSpeech implements TextToSpeech {
    @Override
    public byte[] synthesize(String text) {
        return null;
    }
}
