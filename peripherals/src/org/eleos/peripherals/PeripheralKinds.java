// SPDX-License-Identifier: Apache-2.0
package org.eleos.peripherals;

/** USB class code classification. Descriptor claims are not driver support. */
final class PeripheralKinds {
    static final int AUDIO = 1;
    static final int INPUT = 2;
    static final int STILL_IMAGE = 4;
    static final int PRINTER = 8;
    static final int STORAGE = 16;
    static final int VIDEO = 32;
    private PeripheralKinds() {}

    static int fromClass(int code) {
        // USB-IF standardized device/interface class codes.
        switch (code) {
            case 0x01: return AUDIO;
            case 0x03: return INPUT;
            case 0x06: return STILL_IMAGE;
            case 0x07: return PRINTER;
            case 0x08: return STORAGE;
            case 0x0e: return VIDEO;
            default: return 0;
        }
    }

    static int classify(int deviceClass, int[] interfaceClasses) {
        int result = fromClass(deviceClass);
        if (interfaceClasses != null) {
            for (int i = 0; i < Math.min(interfaceClasses.length, 64); i++) {
                result |= fromClass(interfaceClasses[i]);
            }
        }
        return result;
    }
}
