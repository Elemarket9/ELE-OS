// SPDX-License-Identifier: Apache-2.0
package org.eleos.peripherals;

/** Foreground-only, single-writer interval accounting. Interface counters
 * are partial traffic observations, not billable traffic or link capacity. */
final class LinkRate {
    static final class Rate {
        final long rx, tx;
        Rate(long rx, long tx) { this.rx = rx; this.tx = tx; }
    }
    private String key;
    private long time, receive, transmit;
    void reset() { key = null; }
    Rate update(String network, long now, long rx, long tx) {
        if (network == null || now < 0 || rx < 0 || tx < 0) {
            reset(); return null;
        }
        if (!network.equals(key) || now <= time || rx < receive || tx < transmit ||
                now - time > 10000) {
            key = network; time = now; receive = rx; transmit = tx;
            return null;
        }
        long interval = now - time;
        if (interval < 1000) return null;
        long dr = rx - receive, dt = tx - transmit;
        time = now; receive = rx; transmit = tx;
        if (dr > Long.MAX_VALUE / 1000 || dt > Long.MAX_VALUE / 1000) return null;
        return new Rate(dr * 1000 / interval, dt * 1000 / interval);
    }
}
