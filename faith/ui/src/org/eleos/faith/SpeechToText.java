// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/**
 * On-device speech recognition. PCM is little-endian signed 16-bit mono.
 * A null return means this engine did not produce a transcript. Callers
 * must not send that audio to a cloud provider as a fallback.
 */
public interface SpeechToText {
    String transcribe(byte[] pcmLe16, int sampleRateHz);
}
