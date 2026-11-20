// Copyright 2026 The ELE-OS Authors
// SPDX-License-Identifier: Apache-2.0

package org.eleos.faith;

/**
 * On-device speech synthesis. The result is little-endian signed 16-bit mono
 * PCM, or null when this engine cannot speak.
 */
public interface TextToSpeech {
    byte[] synthesize(String text);
}
