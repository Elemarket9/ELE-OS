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

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/** Which installed apps get a network switch, and whether that switch is on. */
public final class NetworkApps {
    public static final String INTERNET = "android.permission.INTERNET";

    private NetworkApps() {
    }

    public static boolean requestsInternet(String[] permissions) {
        if (permissions == null) {
            return false;
        }
        for (int i = 0; i < permissions.length; i++) {
            if (INTERNET.equals(permissions[i])) {
                return true;
            }
        }
        return false;
    }

    /**
     * Apps that declare {@link #INTERNET} and have a usable package name.
     * A missing policy entry is on. Labels sort case-insensitively, then
     * by package name.
     */
    public static List<AppNetwork> rows(List<AppRecord> installed, NetworkPolicy policy) {
        if (installed == null || policy == null) {
            throw new IllegalArgumentException("missing input");
        }
        ArrayList<AppNetwork> rows = new ArrayList<>();
        HashSet<String> seen = new HashSet<>();
        for (int i = 0; i < installed.size(); i++) {
            AppRecord app = installed.get(i);
            if (app == null || !app.requestsInternet) {
                continue;
            }
            if (!NetworkPolicy.isPackageName(app.packageName) || !seen.add(app.packageName)) {
                continue;
            }
            String label = app.label == null ? "" : app.label.trim();
            if (label.isEmpty()) {
                label = app.packageName;
            }
            boolean on = policy.decide(app.packageName) == NetworkPolicy.Access.GRANTED;
            rows.add(new AppNetwork(app.packageName, label, on));
        }
        rows.sort((left, right) -> {
            int byLabel = left.label.compareToIgnoreCase(right.label);
            if (byLabel != 0) {
                return byLabel;
            }
            return left.packageName.compareTo(right.packageName);
        });
        return rows;
    }
}
