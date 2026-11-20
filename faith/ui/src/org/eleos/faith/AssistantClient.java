// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/**
 * The Faith assistant service. Another package implements this.
 *
 * Implementations answer in text. They do not receive microphone audio.
 * This voice package does not place calls, send messages, set alarms,
 * change settings, or launch apps. Those tools stay in the assistant service,
 * behind the owner's own switches.
 */
public interface AssistantClient {
    AssistantReply ask(AssistantTurn turn);
}
