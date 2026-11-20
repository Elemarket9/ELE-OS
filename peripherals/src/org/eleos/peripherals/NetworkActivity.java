// SPDX-License-Identifier: Apache-2.0
package org.eleos.peripherals;

import android.app.Activity;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.TrafficStats;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.Settings;
import android.widget.TextView;
import java.util.ArrayList;

/** Local passive default-route diagnostic sampler: no sockets, probes,
 * location, DNS hostnames, subscriber IDs, or packet capture. */
public final class NetworkActivity extends Activity {
    private final Handler main = new Handler(Looper.getMainLooper());
    private final LinkRate rate = new LinkRate();
    private ConnectivityManager manager;
    private TextView report;
    private boolean started;
    private final Runnable tick = new Runnable() {
        @Override public void run() {
            if (!started) return;
            refresh();
            main.postDelayed(this, 2000);
        }
    };
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        setContentView(R.layout.network);
        manager = getSystemService(ConnectivityManager.class);
        report = findViewById(R.id.net_report);
        findViewById(R.id.net_settings).setOnClickListener(v ->
                startActivity(new Intent(Settings.ACTION_WIRELESS_SETTINGS)));
    }
    private String word(boolean yes) { return getString(yes ? R.string.net_yes : R.string.net_no); }
    private void refresh() {
        try {
            Network n = manager == null ? null : manager.getActiveNetwork();
            if (n == null) { rate.reset(); report.setText(R.string.net_none); return; }
            NetworkCapabilities c = manager.getNetworkCapabilities(n);
            LinkProperties lp = manager.getLinkProperties(n);
            if (c == null || lp == null || !n.equals(manager.getActiveNetwork())) {
                rate.reset(); report.setText(R.string.net_changing); return;
            }
            int[] transports = {NetworkCapabilities.TRANSPORT_WIFI, NetworkCapabilities.TRANSPORT_CELLULAR,
                    NetworkCapabilities.TRANSPORT_ETHERNET, NetworkCapabilities.TRANSPORT_BLUETOOTH,
                    NetworkCapabilities.TRANSPORT_VPN, NetworkCapabilities.TRANSPORT_USB,
                    NetworkCapabilities.TRANSPORT_SATELLITE};
            int[] labels = {R.string.net_wifi, R.string.net_cellular, R.string.net_ethernet,
                    R.string.net_bluetooth, R.string.net_vpn, R.string.net_usb, R.string.net_satellite};
            ArrayList<String> found = new ArrayList<>();
            for (int i=0; i<transports.length; i++) {
                if(c.hasTransport(transports[i])) found.add(getString(labels[i]));
            }
            String transport = found.isEmpty() ? getString(R.string.net_unknown) :
                    android.icu.text.ListFormatter.getInstance().format(found);
            String iface = lp.getInterfaceName();
            LinkRate.Rate observed = null;
            if (iface != null) {
                observed = rate.update(n.toString() + ":" + iface, SystemClock.elapsedRealtime(),
                        TrafficStats.getRxBytes(iface), TrafficStats.getTxBytes(iface));
            } else rate.reset();
            if (!n.equals(manager.getActiveNetwork())) {
                rate.reset(); report.setText(R.string.net_changing); return;
            }
            StringBuilder out = new StringBuilder(getString(R.string.net_transport, transport));
            out.append('\n').append(getString(R.string.net_validated, word(c.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED))));
            out.append('\n').append(getString(R.string.net_metered, word(!c.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED))));
            out.append('\n').append(getString(R.string.net_captive, word(c.hasCapability(NetworkCapabilities.NET_CAPABILITY_CAPTIVE_PORTAL))));
            out.append('\n').append(getString(R.string.net_private_dns, word(lp.isPrivateDnsActive())));
            out.append('\n').append(getString(R.string.net_dns_count, lp.getDnsServers().size()));
            out.append('\n').append(lp.getMtu() > 0 ? getString(R.string.net_mtu, lp.getMtu()) : getString(R.string.net_mtu_unknown));
            out.append('\n').append(observed == null ? getString(R.string.net_rate_unknown) :
                    getString(R.string.net_rate, observed.rx, observed.tx));
            report.setText(out.toString());
        } catch (RuntimeException ex) { rate.reset(); report.setText(R.string.net_unavailable); }
    }
    @Override protected void onStart() {
        super.onStart(); started = true; main.post(tick);
    }
    @Override protected void onStop() {
        started = false; main.removeCallbacks(tick); rate.reset(); super.onStop();
    }
}
