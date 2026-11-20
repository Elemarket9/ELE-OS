// SPDX-License-Identifier: Apache-2.0
package org.eleos.peripherals;
public final class PowerAdmissionTest {
    static void check(boolean b){if(!b)throw new AssertionError("power admission");}
    public static void main(String[] args){
        check(PowerAdmission.percent(1,3)==33);
        check(PowerAdmission.percent(Integer.MAX_VALUE,Integer.MAX_VALUE)==100);
        check(PowerAdmission.percent(-1,100)==-1);
        check(PowerAdmission.percent(101,100)==-1);
        check(PowerAdmission.percent(0,0)==-1);
        check(PowerAdmission.allows(16,0,false,false));
        check(PowerAdmission.allows(100,1,false,false));
        for(int t=-1;t<10;t++) if(t!=0&&t!=1)check(!PowerAdmission.allows(100,t,false,false));
        check(!PowerAdmission.allows(15,0,false,false));
        check(!PowerAdmission.allows(-1,0,false,false));
        check(!PowerAdmission.allows(101,0,false,false));
        check(!PowerAdmission.allows(100,0,true,false));
        check(!PowerAdmission.allows(100,0,false,true));
        System.out.println("Power policy checks passed");
    }
}
