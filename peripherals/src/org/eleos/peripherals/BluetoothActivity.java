// SPDX-License-Identifier: Apache-2.0
package org.eleos.peripherals;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.text.BidiFormatter;
import android.widget.TextView;
import android.widget.CheckBox;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.Map;

/** Explicit, foreground-only BLE service discovery. No location derivation,
 * background reconnect, persistent addresses, characteristic writes or profile overrides.
 * Optional bounded retries are tied to an explicit foreground request.
 */
public final class BluetoothActivity extends Activity {
    private static final int PERMISSIONS = 8;
    private static final int MAX_SEEN = 64;
    private static final long SCAN_MS = 10_000;
    private static final long OPEN_MS = 25_000;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final LinkQueue queue = new LinkQueue();
    private final Map<String, BluetoothDevice> seen = new LinkedHashMap<>();
    private final Map<String, BluetoothDevice> requested = new LinkedHashMap<>();
    private final Map<String, Session> sessions = new LinkedHashMap<>();
    private final Map<String, RetryBudget> budgets = new LinkedHashMap<>();
    private final Map<String, Runnable> retries = new LinkedHashMap<>();
    private BluetoothAdapter adapter;
    private BluetoothLeScanner scanner;
    private ScanCallback scan;
    private TextView status, links;
    private CheckBox retryEnabled;
    private boolean visible;
    private final Runnable stopScanTask = () -> stopScan(true);
    private final Runnable watch = new Runnable() {
        @Override public void run() {
            if (!visible) return;
            if (!usable() && (scan != null || !queue.ids().isEmpty())) {
                stopScan(false);
                closeAll();
                status.setText(R.string.bt_unavailable);
            }
            main.postDelayed(this, 2000);
        }
    };

    private final class Session extends BluetoothGattCallback {
        final String id;
        BluetoothGatt gatt;
        boolean discovering;
        int services;
        final Runnable timeout;
        Session(String id) {
            this.id = id;
            timeout = () -> { if (current(this)) finish(this, R.string.bt_timeout); };
        }
        @Override public void onConnectionStateChange(BluetoothGatt g, int code, int state) {
            main.post(() -> {
                if (!current(this) || g != gatt) return;
                if (code != BluetoothGatt.GATT_SUCCESS || state == BluetoothProfile.STATE_DISCONNECTED) {
                    finish(this, R.string.bt_disconnected);
                } else if (state == BluetoothProfile.STATE_CONNECTED &&
                        queue.state(id) == LinkQueue.State.OPENING && !discovering) {
                    discovering = true;
                    try {
                        if (!g.discoverServices()) finish(this, R.string.bt_failed);
                    } catch (RuntimeException ex) { finish(this, R.string.bt_failed); }
                }
            });
        }
        @Override public void onServicesDiscovered(BluetoothGatt g, int code) {
            main.post(() -> {
                if (!current(this) || g != gatt || !discovering ||
                        queue.state(id) != LinkQueue.State.OPENING) return;
                if (code != BluetoothGatt.GATT_SUCCESS) { finish(this, R.string.bt_failed); return; }
                try {
                    services = g.getServices().size();
                    if (!queue.ready(id)) { finish(this, R.string.bt_failed); return; }
                    main.removeCallbacks(timeout);
                    status.setText(R.string.bt_ready);
                    render();
                    pump();
                } catch (RuntimeException ex) { finish(this, R.string.bt_failed); }
            });
        }
    }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        setContentView(R.layout.bluetooth);
        status = findViewById(R.id.bt_status);
        links = findViewById(R.id.bt_links);
        retryEnabled = findViewById(R.id.bt_retry);
        // Turning off retries cancels pending retry timers, not ready connections.
        retryEnabled.setOnCheckedChangeListener((button, enabled) -> {
            if (!enabled) {
                for (String id : new ArrayList<>(budgets.keySet())) {
                    RetryBudget budget = budgets.get(id);
                    if (budget != null && budget.hasRetried() &&
                            (queue.state(id) == LinkQueue.State.BACKOFF ||
                             queue.state(id) == LinkQueue.State.QUEUED)) release(id);
                }
                budgets.clear();
                render();
                pump();
            }
        });
        BluetoothManager manager = getSystemService(BluetoothManager.class);
        adapter = manager == null ? null : manager.getAdapter();
        findViewById(R.id.bt_settings).setOnClickListener(v ->
                startActivity(new Intent(Settings.ACTION_BLUETOOTH_SETTINGS)));
        findViewById(R.id.bt_scan).setOnClickListener(v -> withPermission(this::startScan));
        findViewById(R.id.bt_pair).setOnClickListener(v -> withPermission(() -> choose(true)));
        findViewById(R.id.bt_connect).setOnClickListener(v -> withPermission(() -> choose(false)));
        findViewById(R.id.bt_disconnect).setOnClickListener(v -> disconnectOne());
        findViewById(R.id.bt_close_all).setOnClickListener(v -> {
            stopScan(false);
            closeAll();
            status.setText(R.string.bt_closed);
        });
        render();
    }

    private boolean granted() {
        return checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED &&
                checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED;
    }
    private boolean usable() {
        try { return adapter != null && granted() && adapter.isEnabled(); }
        catch (RuntimeException ex) { return false; }
    }
    private void withPermission(Runnable action) {
        if (!granted()) {
            requestPermissions(new String[]{Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT}, PERMISSIONS);
            return;
        }
        if (!usable()) { status.setText(R.string.bt_unavailable); return; }
        action.run();
    }
    @Override public void onRequestPermissionsResult(int code, String[] names, int[] grants) {
        super.onRequestPermissionsResult(code, names, grants);
        if (code == PERMISSIONS) status.setText(granted() ? R.string.bt_permission_ready : R.string.bt_permission_denied);
    }

    private void startScan() {
        if (scan != null || queue.count(LinkQueue.State.OPENING) != 0) {
            status.setText(R.string.bt_wait);
            return;
        }
        seen.clear();
        try {
            scanner = adapter.getBluetoothLeScanner();
            if (scanner == null) { status.setText(R.string.bt_unavailable); return; }
            scan = new ScanCallback() {
                @Override public void onScanResult(int type, ScanResult result) {
                    main.post(() -> {
                        if (!visible || scan != this || !usable()) return;
                        BluetoothDevice d = result.getDevice();
                        String id = d.getAddress();
                        if (seen.size() < MAX_SEEN || seen.containsKey(id)) seen.put(id, d);
                        status.setText(getString(R.string.bt_scanning_count, seen.size()));
                    });
                }
                @Override public void onScanFailed(int code) {
                    main.post(() -> {
                        if (scan != this) return;
                        stopScan(false);
                        status.setText(getString(R.string.bt_scan_error, code));
                    });
                }
            };
            scanner.startScan(scan);
            status.setText(R.string.bt_scanning);
            main.postDelayed(stopScanTask, SCAN_MS);
        } catch (RuntimeException ex) {
            stopScan(false);
            status.setText(R.string.bt_failed);
        }
    }
    private void stopScan(boolean report) {
        main.removeCallbacks(stopScanTask);
        ScanCallback old = scan;
        scan = null;
        if (old != null && scanner != null) {
            try { scanner.stopScan(old); } catch (RuntimeException ignored) { }
        }
        scanner = null;
        if (report) status.setText(getString(R.string.bt_scan_done, seen.size()));
    }

    private String label(BluetoothDevice d) {
        String name = d.getName();
        if (name == null || name.isEmpty()) name = getString(R.string.bt_unnamed);
        // Avoid control/format characters in an untrusted broadcast device name.
        name = name.replaceAll("[\\p{Cc}\\p{Cf}]", "");
        if (name.length() > 60) name = name.substring(0, 60);
        return getString(R.string.bt_device_label, BidiFormatter.getInstance().unicodeWrap(name),
                BidiFormatter.getInstance().unicodeWrap(d.getAddress()));
    }

    private void choose(boolean pairing) {
        stopScan(false);
        if (!visible || !usable()) return;
        if (pairing && queue.count(LinkQueue.State.OPENING) != 0) {
            status.setText(R.string.bt_wait); return;
        }
        try {
            ArrayList<BluetoothDevice> choices = new ArrayList<>();
            Iterable<BluetoothDevice> candidates = pairing ? seen.values() : adapter.getBondedDevices();
            for (BluetoothDevice d : candidates) {
                if (choices.size() >= MAX_SEEN) break;
                boolean bonded = d.getBondState() == BluetoothDevice.BOND_BONDED;
                int type = d.getType();
                if (pairing ? !bonded && d.getBondState() != BluetoothDevice.BOND_BONDING :
                        bonded && (type == BluetoothDevice.DEVICE_TYPE_LE ||
                        type == BluetoothDevice.DEVICE_TYPE_DUAL) && queue.state(d.getAddress()) == null) {
                    choices.add(d);
                }
            }
            if (choices.isEmpty()) { status.setText(R.string.bt_no_choices); return; }
            String[] labels = new String[choices.size()];
            for (int i = 0; i < labels.length; i++) labels[i] = label(choices.get(i));
            new AlertDialog.Builder(this)
                    .setTitle(pairing ? R.string.bt_pair : R.string.bt_connect)
                    .setItems(labels, (dialog, index) -> {
                        if (!visible || !usable()) return;
                        BluetoothDevice device = choices.get(index);
                        try {
                            if (pairing) {
                                // System owns pairing/authentication prompts.
                                status.setText(device.createBond() ? R.string.bt_pair_requested : R.string.bt_failed);
                            } else {
                                String id = device.getAddress();
                                if (device.getBondState() != BluetoothDevice.BOND_BONDED) {
                                    status.setText(R.string.bt_no_choices);
                                    return;
                                }
                                if (!queue.enqueue(id)) { status.setText(R.string.bt_limit); return; }
                                requested.put(id, device);
                                if (retryEnabled.isChecked()) budgets.put(id, new RetryBudget());
                                render();
                                pump();
                            }
                        } catch (RuntimeException ex) { status.setText(R.string.bt_failed); }
                    }).show();
        } catch (RuntimeException ex) { status.setText(R.string.bt_failed); }
    }

    private boolean current(Session s) { return visible && sessions.get(s.id) == s; }

    private void pump() {
        if (!visible || !usable()) { closeAll(); return; }
        stopScan(false);
        String id;
        while ((id = queue.next()) != null) {
            RetryBudget retry = budgets.get(id);
            if (retry != null && retry.hasRetried() && !PowerSnapshot.read(this).allows()) {
                release(id); status.setText(R.string.bt_retry_power); continue;
            }
            Session s = new Session(id);
            sessions.put(id, s);
            try {
                BluetoothDevice device = requested.get(id);
                if (device != null && device.getBondState() == BluetoothDevice.BOND_BONDED) {
                    s.gatt = device.connectGatt(this, false, s, BluetoothDevice.TRANSPORT_LE);
                    if (s.gatt != null) {
                        main.postDelayed(s.timeout, OPEN_MS);
                        render();
                        return;
                    }
                }
            } catch (RuntimeException ignored) { }
            if (!scheduleRetry(id)) {
                release(id);
                status.setText(R.string.bt_failed);
            }
        }
        render();
    }
    private void finish(Session s, int message) {
        if (!current(s)) return;
        if (!scheduleRetry(s.id)) {
            release(s.id);
            status.setText(message);
        }
        render();
        pump();
    }
    private boolean scheduleRetry(String id) {
        RetryBudget budget = budgets.get(id);
        BluetoothDevice device = requested.get(id);
        if (!visible || !usable() || !retryEnabled.isChecked() || budget == null || device == null) return false;
        try {
            if (device.getBondState() != BluetoothDevice.BOND_BONDED) return false;
        } catch (RuntimeException ex) { return false; }
        if (!PowerSnapshot.read(this).allows()) {
            release(id);
            status.setText(R.string.bt_retry_power);
            return true;
        }
        long delay = budget.nextDelay(id.hashCode());
        if (delay < 0 || !queue.defer(id)) return false;
        closeSession(id);
        Runnable task = new Runnable() {
            @Override public void run() {
                if (retries.get(id) != this) return;
                retries.remove(id);
                if (!visible || !usable() || !retryEnabled.isChecked() || !queue.requeue(id)) {
                    release(id);
                    render();
                    return;
                }
                pump();
            }
        };
        retries.put(id, task);
        main.postDelayed(task, delay);
        status.setText(R.string.bt_retry_wait);
        return true;
    }
    private void closeSession(String id) {
        Session s = sessions.remove(id);
        if (s != null) {
            main.removeCallbacks(s.timeout);
            if (s.gatt != null) {
                try { s.gatt.disconnect(); } catch (RuntimeException ignored) { }
                try { s.gatt.close(); } catch (RuntimeException ignored) { }
            }
        }
    }
    private void release(String id) {
        Runnable retry = retries.remove(id);
        if (retry != null) main.removeCallbacks(retry);
        budgets.remove(id);
        queue.remove(id);
        requested.remove(id);
        closeSession(id);
    }
    private void closeAll() {
        for (String id : queue.ids()) release(id);
        queue.clear();
        requested.clear();
        render();
    }
    private void disconnectOne() {
        ArrayList<String> ids = new ArrayList<>(queue.ids());
        if (ids.isEmpty()) { status.setText(R.string.bt_no_choices); return; }
        String[] labels = new String[ids.size()];
        for (int i = 0; i < labels.length; i++) {
            try { labels[i] = label(requested.get(ids.get(i))); }
            catch (RuntimeException ex) { labels[i] = getString(R.string.bt_unnamed); }
        }
        new AlertDialog.Builder(this).setTitle(R.string.bt_disconnect)
                .setItems(labels, (dialog, i) -> {
                    release(ids.get(i));
                    render();
                    pump();
                }).show();
    }
    private void render() {
        StringBuilder out = new StringBuilder(getString(R.string.bt_summary,
                queue.count(LinkQueue.State.READY), queue.count(LinkQueue.State.OPENING),
                queue.count(LinkQueue.State.QUEUED), queue.count(LinkQueue.State.BACKOFF)));
        for (String id : queue.ids()) {
            Session s = sessions.get(id);
            LinkQueue.State state = queue.state(id);
            String name;
            try { name = label(requested.get(id)); }
            catch (RuntimeException ex) { name = getString(R.string.bt_unnamed); }
            out.append('\n').append(name).append(" — ");
            if (state == LinkQueue.State.READY && s != null) out.append(getString(R.string.bt_services, s.services));
            else out.append(getString(state == LinkQueue.State.OPENING ? R.string.bt_opening :
                    state == LinkQueue.State.BACKOFF ? R.string.bt_retry_wait : R.string.bt_queued));
        }
        links.setText(out.toString());
    }

    @Override protected void onStart() {
        super.onStart();
        visible = true;
        main.post(watch);
    }
    @Override protected void onStop() {
        visible = false;
        main.removeCallbacks(watch);
        stopScan(false);
        closeAll();
        seen.clear();
        super.onStop();
    }
}
