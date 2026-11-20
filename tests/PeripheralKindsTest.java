// SPDX-License-Identifier: Apache-2.0
package org.eleos.peripherals;

public final class PeripheralKindsTest {
    private static void require(boolean condition) {
        if (!condition) throw new AssertionError("USB classification mismatch");
    }
    public static void main(String[] args) {
        require(PeripheralKinds.classify(0, new int[]{0x06, 0x0e}) ==
                (PeripheralKinds.STILL_IMAGE | PeripheralKinds.VIDEO));
        require(PeripheralKinds.classify(0x08, new int[]{0x01, 0x03, 0x07}) ==
                (PeripheralKinds.STORAGE | PeripheralKinds.AUDIO | PeripheralKinds.INPUT |
                PeripheralKinds.PRINTER));
        require(PeripheralKinds.classify(0xff, null) == 0);
        require(PeripheralKinds.classify(-1, new int[]{-1, 256, 0xff}) == 0);
        require(PeripheralKinds.classify(0x0e, new int[]{0x0e, 0x0e}) == PeripheralKinds.VIDEO);
        int[] large = new int[65];
        large[64] = 0x06;
        require(PeripheralKinds.classify(0, large) == 0);
        System.out.println("USB descriptor class tests passed");
    }
}
