// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/**
 * Cloud speech included in this build. It does not open a socket and does
 * not return a transcript. The class has no network API on purpose.
 */
public final class NoNetworkCloudSpeech implements CloudSpeechProvider {
    @Override
    public boolean transmits() {
        return false;
    }

    @Override
    public String transcribe(byte[] pcmLe16, int sampleRateHz, String endpoint) {
        return null;
    }
}
