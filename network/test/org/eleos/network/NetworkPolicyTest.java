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

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Host checks for the network choice. No socket is opened. */
public final class NetworkPolicyTest {
    private static int checks;

    public static void main(String[] args) throws Exception {
        grantByDefault();
        denialIsNetworkDown();
        appsStayIndependent();
        roundTrip();
        corruptFileStaysPut();
        missingFileStaysMissing();
        packageNames();
        permissionScreen();
        if (NetworkPolicy.Access.values().length != 2) {
            throw new RuntimeException("access values");
        }
        check(true, "two access values");
        System.out.println("Network policy checks passed (" + checks + ")");
    }

    private static void grantByDefault() throws Exception {
        NetworkPolicy policy = NetworkPolicy.load("");
        check(policy.decide("com.example.app") == NetworkPolicy.Access.GRANTED, "empty grant");
        check("granted".equals(policy.decide("com.example.app").reason), "grant reason");
        check(!policy.save().contains("com.example.app"), "grant is not a line");
        NetworkPolicy.Observe open = NetworkPolicy.outcomeFor(NetworkPolicy.Access.GRANTED);
        check(open.delivered && open.errno == null, "granted delivery");
        try {
            NetworkPolicy.outcomeFor(null);
            check(false, "null access");
        } catch (IllegalArgumentException ex) {
            check(true, "null access");
        }
    }

    private static void denialIsNetworkDown() throws Exception {
        NetworkPolicy policy = new NetworkPolicy();
        policy.setNetwork("com.example.mail", false);
        check(policy.decide("com.example.mail") == NetworkPolicy.Access.NETWORK_DOWN, "denied");
        check("network-down".equals(policy.decide("com.example.mail").reason), "down reason");
        NetworkPolicy.Observe down = NetworkPolicy.outcomeFor(NetworkPolicy.Access.NETWORK_DOWN);
        check(!down.delivered && "ENETUNREACH".equals(down.errno), "down errno");
        policy.setNetwork("com.example.mail", true);
        check(policy.decide("com.example.mail") == NetworkPolicy.Access.GRANTED, "restored");
        check(!policy.save().contains("com.example.mail"), "restored line removed");
    }

    private static void appsStayIndependent() {
        NetworkPolicy policy = new NetworkPolicy();
        policy.setNetwork("com.example.maps", false);
        policy.setNetwork("com.example.mail", true);
        check(policy.decide("com.example.maps") == NetworkPolicy.Access.NETWORK_DOWN, "maps off");
        check(policy.decide("com.example.mail") == NetworkPolicy.Access.GRANTED, "mail on");
        check(policy.decide("com.example.other") == NetworkPolicy.Access.GRANTED, "other on");
        String saved = policy.save();
        check(saved.startsWith(NetworkPolicy.HEADER + "\n"), "header");
        check(saved.contains("com.example.maps off\n"), "maps line");
        check(!saved.contains("wifi") && !saved.contains("mobile") && !saved.contains("vpn"),
                "one switch");
    }

    private static void roundTrip() throws Exception {
        String text = "# comment\n\nCom.Example.App off\r\ncom.example.other off\n";
        NetworkPolicy loaded = NetworkPolicy.load(text);
        check(loaded.decide("Com.Example.App") == NetworkPolicy.Access.NETWORK_DOWN, "case kept");
        check(loaded.decide("com.example.other") == NetworkPolicy.Access.NETWORK_DOWN, "crlf");
        NetworkPolicy again = NetworkPolicy.load(loaded.save());
        check(again.decide("Com.Example.App") == NetworkPolicy.Access.NETWORK_DOWN, "saved case");
        check(again.decide("com.example.fresh") == NetworkPolicy.Access.GRANTED, "fresh grant");

        Path dir = Files.createTempDirectory("ele-network-policy");
        try {
            Path file = dir.resolve("network-policy.txt");
            loaded.writeTo(file);
            NetworkPolicy fromDisk = NetworkPolicy.read(file);
            check(fromDisk.decide("com.example.other") == NetworkPolicy.Access.NETWORK_DOWN,
                    "disk denial");
            check(fromDisk.save().equals(loaded.save()), "disk text");
            check(Files.notExists(dir.resolve("network-policy.txt.tmp")), "tmp removed");
        } finally {
            Files.deleteIfExists(dir.resolve("network-policy.txt"));
            Files.deleteIfExists(dir.resolve("network-policy.txt.tmp"));
            Files.deleteIfExists(dir);
        }

        try {
            NetworkPolicy.load("com.example.app off\ncom.example.app off\n");
            check(false, "duplicate");
        } catch (PolicyFormatException ex) {
            check(ex.getMessage().contains("2"), "duplicate line");
        }
        try {
            NetworkPolicy.load("com.example.app on\n");
            check(false, "explicit on");
        } catch (PolicyFormatException ex) {
            check(ex.getMessage().contains("1"), "on rejected");
        }
        try {
            NetworkPolicy.load("com.example.app off extra\n");
            check(false, "extra column");
        } catch (PolicyFormatException ex) {
            check(true, "extra column");
        }
    }

    private static void corruptFileStaysPut() throws Exception {
        Path dir = Files.createTempDirectory("ele-network-corrupt");
        try {
            Path file = dir.resolve("network-policy.txt");
            byte[] original = "this is not a policy\n".getBytes(StandardCharsets.UTF_8);
            Files.write(file, original);
            try {
                NetworkPolicy.read(file);
                check(false, "corrupt read");
            } catch (PolicyFormatException ex) {
                check(true, "corrupt read");
            }
            check(Arrays.equals(original, Files.readAllBytes(file)), "corrupt bytes");
        } finally {
            Files.deleteIfExists(dir.resolve("network-policy.txt"));
            Files.deleteIfExists(dir);
        }
        try {
            NetworkPolicy.load(null);
            check(false, "null text");
        } catch (PolicyFormatException ex) {
            check(true, "null text");
        }
        try {
            NetworkPolicy.load("com.example.app off\0\n");
            check(false, "nul");
        } catch (PolicyFormatException ex) {
            check(true, "nul");
        }
    }

    private static void missingFileStaysMissing() throws Exception {
        Path dir = Files.createTempDirectory("ele-network-missing");
        try {
            Path file = dir.resolve("network-policy.txt");
            NetworkPolicy policy = NetworkPolicy.read(file);
            check(policy.decide("com.example.app") == NetworkPolicy.Access.GRANTED, "absent grant");
            check(Files.notExists(file), "absent file");
        } finally {
            Files.deleteIfExists(dir);
        }
    }

    private static void packageNames() {
        check(NetworkPolicy.isPackageName("android"), "platform package");
        check(NetworkPolicy.isPackageName("com.example.app"), "app package");
        check(NetworkPolicy.isPackageName("com.example.my_app"), "underscore");
        check(!NetworkPolicy.isPackageName(""), "empty");
        check(!NetworkPolicy.isPackageName("com..app"), "empty segment");
        check(!NetworkPolicy.isPackageName(".com.app"), "leading dot");
        check(!NetworkPolicy.isPackageName("com.example.app "), "space");
        check(!NetworkPolicy.isPackageName("not a package"), "words");
        check(!NetworkPolicy.isPackageName("com.1example.app"), "digit segment");
        check(!NetworkPolicy.isPackageName("a".repeat(256)), "too long");
        NetworkPolicy policy = new NetworkPolicy();
        try {
            policy.setNetwork("not a package", false);
            check(false, "reject bad set");
        } catch (IllegalArgumentException ex) {
            check(true, "reject bad set");
        }
        try {
            policy.decide("../x");
            check(false, "reject bad decide");
        } catch (IllegalArgumentException ex) {
            check(true, "reject bad decide");
        }
    }

    private static void permissionScreen() {
        check(NetworkApps.requestsInternet(new String[] {"android.permission.INTERNET"}), "internet");
        check(!NetworkApps.requestsInternet(null), "null perms");
        check(!NetworkApps.requestsInternet(new String[] {}), "no perms");
        check(!NetworkApps.requestsInternet(new String[] {"android.permission.internet"}), "case");
        check(!NetworkApps.requestsInternet(new String[] {"android.permission.INTERNET2"}), "prefix");

        NetworkPolicy policy = new NetworkPolicy();
        policy.setNetwork("com.example.maps", false);
        policy.setNetwork("com.hidden.app", false);
        List<AppRecord> installed = new ArrayList<>();
        installed.add(new AppRecord("com.example.mail", "Mail", true));
        installed.add(new AppRecord("com.example.maps", "Maps", true));
        installed.add(new AppRecord("com.example.camera", "Camera", false));
        installed.add(new AppRecord("com.hidden.app", "Hidden", false));
        installed.add(new AppRecord("not a package", "Broken", true));
        installed.add(new AppRecord("com.example.mail", "Mail duplicate", true));
        installed.add(new AppRecord("com.example.notes", "  ", true));
        installed.add(null);
        installed.add(new AppRecord("com.example.beta", "beta", true));
        installed.add(new AppRecord("com.example.alpha", "Alpha", true));

        List<AppNetwork> rows = NetworkApps.rows(installed, policy);
        check(rows.size() == 5, "row count " + rows.size());
        check("Alpha".equals(rows.get(0).label) && rows.get(0).networkOn, "alpha on");
        check("beta".equals(rows.get(1).label) && rows.get(1).networkOn, "beta on");
        check("com.example.notes".equals(rows.get(2).label) && rows.get(2).networkOn, "blank label");
        check("Mail".equals(rows.get(3).label) && rows.get(3).networkOn, "mail first label");
        check("com.example.mail".equals(rows.get(3).packageName), "mail package");
        check("Maps".equals(rows.get(4).label) && !rows.get(4).networkOn, "maps off");
        check(policy.decide("com.hidden.app") == NetworkPolicy.Access.NETWORK_DOWN, "hidden kept");

        try {
            NetworkApps.rows(null, policy);
            check(false, "null apps");
        } catch (IllegalArgumentException ex) {
            check(true, "null apps");
        }
    }

    private static void check(boolean ok, String message) {
        if (!ok) {
            throw new RuntimeException(message);
        }
        checks++;
    }
}
