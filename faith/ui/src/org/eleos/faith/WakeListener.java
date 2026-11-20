// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/**
 * On-device wake detector. It must be aimable at the phrase the owner set.
 * The product engine is not chosen. The only listener in this build never
 * reports a hit.
 */
public interface WakeListener {
    void arm(String phrase);

    void release();

    /** True only when this buffer contains the armed phrase. */
    boolean accept(byte[] pcmLe16, int sampleRateHz);
}
