// SPDX-License-Identifier: Apache-2.0
package org.eleos.peripherals;
public final class LinkRateTest {
    static void check(boolean good) { if (!good) throw new AssertionError("rate invariant"); }
    public static void main(String[] args) {
        LinkRate r = new LinkRate();
        check(r.update("wlan",1000,100,100)==null);
        check(r.update("wlan",1500,150,300)==null);
        LinkRate.Rate rate=r.update("wlan",3000,1100,4100);
        check(rate!=null && rate.rx==500 && rate.tx==2000);
        check(r.update("cell",5000,9000,9000)==null);
        check(r.update("cell",7000,10,10)==null);
        check(r.update("cell",8000,-1,800)==null);
        check(r.update("cell",9000,1,2)==null);
        check(r.update("cell",20000,2,2)==null);
        check(r.update("cell",21000,Long.MAX_VALUE,3)==null);
        r.reset(); check(r.update("cell",22000,0,0)==null);
        check(r.update("cell",21000,1,1)==null);
        System.out.println("Network interface interval checks passed");
    }
}
