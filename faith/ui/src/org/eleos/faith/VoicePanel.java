// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/**
 * Status the voice screen shows. LISTENING_MIC_CLOSED is accurate only while
 * this build does not open a microphone. OPTED_IN_NOTHING_SENT is accurate
 * only while the installed provider does not transmit.
 */
public final class VoicePanel {
    public enum Mic {
        IDLE, LISTENING_MIC_CLOSED
    }

    public enum CloudLine {
        OFF, NEEDS_ENDPOINT, OPTED_IN_NOTHING_SENT, OPTED_IN_CAN_SEND
    }

    private VoicePanel() {}

    public static Mic mic(VoiceSession session) {
        return session.captureEnabled() ? Mic.LISTENING_MIC_CLOSED : Mic.IDLE;
    }

    public static CloudLine cloud(VoiceSession session) {
        switch (session.cloudGate()) {
            case NO_ENDPOINT:
                return CloudLine.NEEDS_ENDPOINT;
            case OPTED_IN:
                return session.cloudTransmits() ? CloudLine.OPTED_IN_CAN_SEND : CloudLine.OPTED_IN_NOTHING_SENT;
            case OFF:
            default:
                return CloudLine.OFF;
        }
    }
}
