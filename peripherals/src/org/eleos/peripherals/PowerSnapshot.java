// SPDX-License-Identifier: Apache-2.0
package org.eleos.peripherals;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.PowerManager;

/** Read-only snapshot of the platform's reported battery and thermal state. */
final class PowerSnapshot {
    int percent=-1, thermal=-1;
    int temperature=Integer.MIN_VALUE, voltage=-1, current=Integer.MIN_VALUE;
    boolean saver=true, low=true;
    boolean allows() { return PowerAdmission.allows(percent, thermal, saver, low); }

    static PowerSnapshot read(Context context) {
        PowerSnapshot out = new PowerSnapshot();
        try {
            PowerManager pm=context.getSystemService(PowerManager.class);
            if (pm!=null) {
                out.thermal=pm.getCurrentThermalStatus();
                out.saver=pm.isPowerSaveMode();
            }
            Intent battery=context.registerReceiver(null,new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
            if(battery!=null && battery.getBooleanExtra(BatteryManager.EXTRA_PRESENT,false)) {
                out.percent=PowerAdmission.percent(battery.getIntExtra(BatteryManager.EXTRA_LEVEL,-1),
                        battery.getIntExtra(BatteryManager.EXTRA_SCALE,-1));
                out.low=battery.getBooleanExtra(BatteryManager.EXTRA_BATTERY_LOW,true);
                out.temperature=battery.getIntExtra(BatteryManager.EXTRA_TEMPERATURE,Integer.MIN_VALUE);
                out.voltage=battery.getIntExtra(BatteryManager.EXTRA_VOLTAGE,-1);
                BatteryManager bm=context.getSystemService(BatteryManager.class);
                if(bm!=null) out.current=bm.getIntProperty(BatteryManager.BATTERY_PROPERTY_CURRENT_NOW);
            }
        } catch(RuntimeException ex) { return new PowerSnapshot(); }
        return out;
    }
}
