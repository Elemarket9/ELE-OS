// SPDX-License-Identifier: Apache-2.0
package org.eleos.peripherals;
/** Conservative admission for this tool's optional automatic work only.
 * Thermal codes are the Android PowerManager values NONE=0 and LIGHT=1.
 * Never override platform, device, or urgent-work rules. */
final class PowerAdmission {
    static boolean allows(int percent, int thermal, boolean saver, boolean low) {
        return percent>15 && percent<=100 && (thermal==0 || thermal==1) && !saver && !low;
    }
    static int percent(int level, int scale) {
        if (level<0 || scale<=0 || level>scale) return -1;
        return (int)((100L*level)/scale);
    }
}
