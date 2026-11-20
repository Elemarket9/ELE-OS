// SPDX-License-Identifier: Apache-2.0
package org.eleos.peripherals;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.TextView;

public final class PowerActivity extends Activity {
    private final Handler main=new Handler(Looper.getMainLooper());
    private TextView report;
    private boolean visible;
    private final Runnable tick=new Runnable() {
        @Override public void run() {
            if(!visible)return;
            refresh();
            main.postDelayed(this,5000);
        }
    };
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        setContentView(R.layout.power);
        report=findViewById(R.id.power_report);
    }
    private String unknown(){return getString(R.string.power_unknown);}
    private String yes(boolean value){return getString(value?R.string.net_yes:R.string.net_no);}
    private void refresh(){
        PowerSnapshot s=PowerSnapshot.read(this);
        String battery=s.percent<0?unknown():getString(R.string.power_percent,s.percent);
        String temperature=s.temperature==Integer.MIN_VALUE?unknown():getString(R.string.power_temp,s.temperature/10.0);
        String voltage=s.voltage<=0?unknown():getString(R.string.power_voltage,s.voltage);
        String current=s.current==Integer.MIN_VALUE?unknown():getString(R.string.power_current,s.current);
        String thermal;
        switch(s.thermal) {
            case 0: thermal=getString(R.string.power_none); break;
            case 1: thermal=getString(R.string.power_light); break;
            case 2: thermal=getString(R.string.power_moderate); break;
            case 3: thermal=getString(R.string.power_severe); break;
            case 4: thermal=getString(R.string.power_critical); break;
            case 5: thermal=getString(R.string.power_emergency); break;
            case 6: thermal=getString(R.string.power_shutdown); break;
            default: thermal=unknown();
        }
        report.setText(getString(R.string.power_report,battery,temperature,voltage,current,thermal,
            s.thermal<0?unknown():yes(s.saver),yes(s.allows())));
    }
    @Override protected void onStart(){super.onStart();visible=true;main.post(tick);}
    @Override protected void onStop(){visible=false;main.removeCallbacks(tick);super.onStop();}
}
