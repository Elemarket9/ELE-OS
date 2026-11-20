// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/**
 * Owner-configured cloud speech. No provider is built in.
 * The voice session calls this only after the owner opts in and the endpoint
 * is accepted. A failure here must not fall through to another network.
 */
public interface CloudSpeechProvider {
    /** True only when this implementation can send audio off the device. */
    boolean transmits();

    String transcribe(byte[] pcmLe16, int sampleRateHz, String endpoint);
}
