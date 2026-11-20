// SPDX-License-Identifier: Apache-2.0
package org.eleos.peripherals;
/** Three retries over a single explicit connection request, not per outage.
 * Delay is exponentially bounded with up to 250ms deterministic peer jitter. */
final class RetryBudget {
    private int attempts;
    long nextDelay(int entropy) {
        if (attempts >= 3) return -1;
        long base = 1000L << attempts++;
        return base + Math.floorMod(entropy, 251);
    }
}
