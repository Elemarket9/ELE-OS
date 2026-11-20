// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/** Used until the assistant service is connected. It does not invent an answer. */
public final class MissingAssistantClient implements AssistantClient {
    public static final String MESSAGE = "The Faith assistant service is not connected.";

    @Override
    public AssistantReply ask(AssistantTurn turn) {
        return new AssistantReply(MESSAGE, false);
    }
}
