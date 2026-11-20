// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/**
 * Wake listener used until an engine is chosen. It stores the phrase the
 * owner set and never reports a hit, so it cannot pretend to have heard it.
 */
public final class UnavailableWakeListener implements WakeListener {
    private String phrase = "";
    private boolean released = true;

    @Override
    public void arm(String phrase) {
        this.phrase = phrase == null ? "" : phrase;
        released = false;
    }

    @Override
    public void release() {
        released = true;
    }

    @Override
    public boolean accept(byte[] pcmLe16, int sampleRateHz) {
        return false;
    }

    public String armedPhrase() {
        return phrase;
    }

    public boolean released() {
        return released;
    }
}
