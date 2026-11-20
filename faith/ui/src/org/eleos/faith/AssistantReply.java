// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/** Text answer from the Faith assistant service. */
public final class AssistantReply {
    private final String text;
    private final boolean complete;

    public AssistantReply(String text, boolean complete) {
        this.text = text == null ? "" : text;
        this.complete = complete && !this.text.isBlank();
    }

    public String text() {
        return text;
    }

    /** False when the service is absent or the answer is empty. Speech is not synthesized then. */
    public boolean complete() {
        return complete;
    }
}
