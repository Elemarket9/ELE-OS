/*
 * Copyright 2026 Sweet Meet
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.eleos.network;

import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.CompoundButton;
import android.widget.ListView;
import android.widget.Switch;
import android.widget.TextView;
import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

/**
 * Launcher screen for the per-app network switch. The choice is stored in
 * this app's private {@code network-policy.txt}. The screen does not install
 * a packet filter.
 */
public final class NetworkPermissionActivity extends Activity {
    private final ArrayList<AppNetwork> rows = new ArrayList<>();
    private NetworkPolicy policy;
    private RowAdapter adapter;
    private TextView statusView;
    private ListView listView;

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        setContentView(R.layout.activity_network);
        statusView = findViewById(R.id.status);
        listView = findViewById(R.id.apps);
        adapter = new RowAdapter();
        listView.setAdapter(adapter);
        try {
            policy = NetworkPolicy.read(policyFile());
        } catch (PolicyFormatException | IOException ex) {
            policy = null;
            showStatus(R.string.policy_unreadable, true);
            return;
        }
        statusView.setText(R.string.loading);
        new Thread(this::loadApps, "ele-network-apps").start();
    }

    private void loadApps() {
        final List<AppRecord> installed;
        try {
            installed = queryApps();
        } catch (RuntimeException ex) {
            runOnUiThread(() -> showStatus(R.string.apps_unavailable, true));
            return;
        }
        runOnUiThread(() -> {
            if (isDestroyed() || policy == null) {
                return;
            }
            try {
                show(NetworkApps.rows(installed, policy));
            } catch (RuntimeException ex) {
                showStatus(R.string.apps_unavailable, true);
            }
        });
    }

    private List<AppRecord> queryApps() {
        PackageManager packages = getPackageManager();
        List<PackageInfo> infos = packages.getInstalledPackages(PackageManager.GET_PERMISSIONS);
        ArrayList<AppRecord> installed = new ArrayList<>();
        for (int i = 0; i < infos.size(); i++) {
            PackageInfo info = infos.get(i);
            if (info.packageName == null) {
                continue;
            }
            String label = null;
            ApplicationInfo application = info.applicationInfo;
            if (application != null) {
                CharSequence loaded = application.loadLabel(packages);
                if (loaded != null) {
                    label = loaded.toString();
                }
            }
            installed.add(new AppRecord(info.packageName, label,
                    NetworkApps.requestsInternet(info.requestedPermissions)));
        }
        return installed;
    }

    private void show(List<AppNetwork> next) {
        rows.clear();
        rows.addAll(next);
        adapter.notifyDataSetChanged();
        listView.setVisibility(View.VISIBLE);
        if (next.isEmpty()) {
            statusView.setVisibility(View.VISIBLE);
            statusView.setText(R.string.empty_apps);
        } else {
            statusView.setVisibility(View.GONE);
        }
    }

    private void showStatus(int resId, boolean hideList) {
        if (isDestroyed()) {
            return;
        }
        statusView.setVisibility(View.VISIBLE);
        statusView.setText(resId);
        listView.setVisibility(hideList ? View.GONE : View.VISIBLE);
    }

    private void onSwitch(String packageName, boolean on) {
        if (policy == null) {
            return;
        }
        boolean previous = policy.decide(packageName) == NetworkPolicy.Access.GRANTED;
        try {
            policy.setNetwork(packageName, on);
            policy.writeTo(policyFile());
        } catch (IOException | RuntimeException ex) {
            try {
                policy.setNetwork(packageName, previous);
            } catch (RuntimeException ignored) {
                // setNetwork rejected the name before the file was written.
            }
            showStatus(R.string.save_failed, false);
            adapter.notifyDataSetChanged();
            return;
        }
        for (int i = 0; i < rows.size(); i++) {
            AppNetwork row = rows.get(i);
            if (packageName.equals(row.packageName)) {
                rows.set(i, new AppNetwork(row.packageName, row.label, on));
                break;
            }
        }
        adapter.notifyDataSetChanged();
        if (rows.isEmpty()) {
            statusView.setVisibility(View.VISIBLE);
            statusView.setText(R.string.empty_apps);
        } else {
            statusView.setVisibility(View.GONE);
        }
    }

    private Path policyFile() {
        return getFilesDir().toPath().resolve("network-policy.txt");
    }

    private final class RowAdapter extends BaseAdapter {
        @Override
        public int getCount() {
            return rows.size();
        }

        @Override
        public Object getItem(int position) {
            return rows.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            Holder holder;
            if (convertView == null) {
                convertView = LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.app_network_row, parent, false);
                holder = new Holder(convertView);
                convertView.setTag(holder);
            } else {
                holder = (Holder) convertView.getTag();
            }
            AppNetwork item = rows.get(position);
            holder.label.setText(item.label);
            holder.state.setText(item.networkOn ? R.string.state_on : R.string.state_off);
            holder.network.setOnCheckedChangeListener(null);
            holder.network.setChecked(item.networkOn);
            holder.network.setContentDescription(getString(
                    R.string.switch_content_description, item.label));
            holder.network.setOnCheckedChangeListener(
                    (CompoundButton button, boolean checked) -> onSwitch(item.packageName, checked));
            return convertView;
        }
    }

    private static final class Holder {
        final TextView label;
        final TextView state;
        final Switch network;

        Holder(View row) {
            label = row.findViewById(R.id.label);
            state = row.findViewById(R.id.state);
            network = row.findViewById(R.id.network_switch);
        }
    }
}
