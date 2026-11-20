// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/** Text handed to the Faith assistant service. This package does not run a model. */
public final class AssistantTurn {
    private final String text;
    private final boolean fromVoice;

    public AssistantTurn(String text, boolean fromVoice) {
        if (text == null) {
            throw new IllegalArgumentException("text");
        }
        this.text = text;
        this.fromVoice = fromVoice;
    }

    public String text() {
        return text;
    }

    public boolean fromVoice() {
        return fromVoice;
    }
}
