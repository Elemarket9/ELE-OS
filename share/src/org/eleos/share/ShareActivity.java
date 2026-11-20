package org.eleos.share;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.net.wifi.p2p.WifiP2pConfig;
import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pDeviceList;
import android.net.wifi.p2p.WifiP2pManager;
import android.os.Build;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import org.eleos.share.protocol.TransferProtocol;

/** Built-in peer sharing using an authenticated, streaming data channel. */
@SuppressWarnings("deprecation")
public final class ShareActivity extends Activity {
    private static final int REQUEST_NEARBY = 40;
    private static final int REQUEST_DESTINATION = 41;
    private static final int REQUEST_LOCAL_NETWORK = 42;
    private static final String ACCESS_LOCAL_NETWORK = "android.permission.ACCESS_LOCAL_NETWORK";
    private static final String SERVICE_TYPE = "_ele-share._tcp.";
    private static final int SERVICE_PORT = 45876;
    private WifiP2pManager manager;
    private WifiP2pManager.Channel channel;
    private NsdManager nsdManager;
    private BroadcastReceiver receiver;
    private boolean receiverRegistered;
    private volatile boolean sessionStarting;
    private volatile boolean cancelled;
    private boolean transportsStarted;
    private final List<WifiP2pDevice> peers = new ArrayList<>();
    private final List<Uri> offeredUris = new ArrayList<>();
    private LinearLayout peerList;
    private TextView status;
    private Uri destinationTree;
    private volatile AndroidTlsPairing pairing;
    private volatile String transportName = "secure peer link";
    private ServerSocket transportServer;
    private final ExecutorService worker = new ThreadPoolExecutor(1, 1, 0L,
            TimeUnit.MILLISECONDS, new ArrayBlockingQueue<>(1));
    private final ExecutorService acceptWorker = new ThreadPoolExecutor(1, 1, 0L,
            TimeUnit.MILLISECONDS, new ArrayBlockingQueue<>(1));
    private final Map<String, NsdServiceInfo> lanPeers = new HashMap<>();
    private NsdManager.DiscoveryListener discoveryListener;
    private NsdManager.RegistrationListener registrationListener;
    private volatile String ownServiceName;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(24, 24, 24, 24);
        TextView title = new TextView(this);
        title.setText("ELE Share");
        title.setTextSize(28);
        root.addView(title);
        TextView speedNote = new TextView(this);
        speedNote.setText("30 GB in 30 seconds needs about 8 Gbit/s sustained payload speed. ELE Share measures each completed transfer; actual speed depends on the radios, wired link, storage, and both devices.");
        root.addView(speedNote);
        status = new TextView(this);
        root.addView(status);
        Button discover = new Button(this);
        discover.setText("Find nearby ELE devices");
        discover.setOnClickListener(v -> beginDiscovery());
        root.addView(discover);
        Button destination = new Button(this);
        destination.setText("Choose receive folder");
        destination.setOnClickListener(v -> chooseDestination());
        root.addView(destination);
        peerList = new LinearLayout(this);
        peerList.setOrientation(LinearLayout.VERTICAL);
        root.addView(peerList);
        ScrollView scroll = new ScrollView(this);
        scroll.addView(root);
        setContentView(scroll);
        collectShareIntent(getIntent());
        manager = (WifiP2pManager) getSystemService(Context.WIFI_P2P_SERVICE);
        nsdManager = (NsdManager) getSystemService(Context.NSD_SERVICE);
        if (manager != null) {
            channel = manager.initialize(this, getMainLooper(), reason -> setStatus("Nearby service disconnected. Reopen discovery."));
            receiver = new P2pReceiver();
        }
        requestNearbyPermission();
    }

    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        collectShareIntent(intent);
    }

    @Override protected void onResume() {
        super.onResume();
        if (receiver != null && !receiverRegistered) {
            IntentFilter filter = new IntentFilter();
            filter.addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION);
            filter.addAction(WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION);
            filter.addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION);
            filter.addAction(WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION);
            if (Build.VERSION.SDK_INT >= 33) registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED);
            else registerReceiver(receiver, filter);
            receiverRegistered = true;
        }
    }

    @Override protected void onPause() {
        if (receiverRegistered) {
            unregisterReceiver(receiver);
            receiverRegistered = false;
        }
        super.onPause();
    }

    private void requestNearbyPermission() {
        if (Build.VERSION.SDK_INT >= 37
                && checkSelfPermission(ACCESS_LOCAL_NETWORK) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[] {ACCESS_LOCAL_NETWORK}, REQUEST_LOCAL_NETWORK);
            return;
        }
        if (manager != null && Build.VERSION.SDK_INT >= 33
                && checkSelfPermission(Manifest.permission.NEARBY_WIFI_DEVICES) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[] {Manifest.permission.NEARBY_WIFI_DEVICES}, REQUEST_NEARBY);
            return;
        }
        if (!transportsStarted) {
            transportsStarted = true;
            startLanTransport();
            startLanDiscovery();
        }
        setStatus(manager == null
                ? "Ready on local and wired networks. Direct Wi-Fi peer discovery is unavailable."
                : "Ready. Share a file from another app, or choose a receive folder and connect to a nearby device.");
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == REQUEST_NEARBY || requestCode == REQUEST_LOCAL_NETWORK) {
            boolean granted = results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED;
            if (!granted) {
                setStatus("Nearby network permission is needed to discover peers and open a direct transfer link.");
                return;
            }
            requestNearbyPermission();
        }
    }

    @SuppressWarnings("deprecation")
    private void collectShareIntent(Intent intent) {
        if (intent == null) return;
        String action = intent.getAction();
        if (Intent.ACTION_SEND.equals(action)) {
            Uri uri = Build.VERSION.SDK_INT >= 33
                    ? intent.getParcelableExtra(Intent.EXTRA_STREAM, Uri.class)
                    : intent.getParcelableExtra(Intent.EXTRA_STREAM);
            if (uri != null) offeredUris.add(uri);
        } else if (Intent.ACTION_SEND_MULTIPLE.equals(action)) {
            ArrayList<Uri> uris = Build.VERSION.SDK_INT >= 33
                    ? intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM, Uri.class)
                    : intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
            if (uris != null) offeredUris.addAll(uris);
        }
        if (status != null && !offeredUris.isEmpty()) {
            setStatus(offeredUris.size() + " item(s) ready to send. Select the intended device to continue.");
        }
    }

    private void chooseDestination() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(intent, REQUEST_DESTINATION);
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_DESTINATION && resultCode == RESULT_OK && data != null) {
            destinationTree = data.getData();
            int flags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            try { getContentResolver().takePersistableUriPermission(destinationTree, flags); }
            catch (SecurityException ignored) { /* Current activity grant remains usable. */ }
            setStatus("Receive folder selected. Connect to the sending device when ready.");
        }
    }

    private void beginDiscovery() {
        if (manager == null || channel == null) return;
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.NEARBY_WIFI_DEVICES) != PackageManager.PERMISSION_GRANTED) {
            requestNearbyPermission();
            return;
        }
        setStatus("Looking for direct Wi-Fi peers…");
        manager.discoverPeers(channel, new WifiP2pManager.ActionListener() {
            @Override public void onSuccess() { setStatus("Scanning for nearby ELE Share peers…"); }
            @Override public void onFailure(int reason) { setStatus("Peer discovery failed (" + reason + "). Check Wi-Fi is enabled."); }
        });
    }

    private void showPeers(WifiP2pDeviceList devices) {
        peers.clear();
        peers.addAll(devices.getDeviceList());
        if (peers.isEmpty()) {
            setStatus("No nearby Wi-Fi peers found. Keep both devices awake and try again.");
        } else {
            setStatus("Choose the intended device. Both users must compare and confirm a pairing code.");
        }
        renderLanPeers();
    }

    private void renderLanPeers() {
        if (peerList == null) return;
        peerList.removeAllViews();
        for (WifiP2pDevice device : peers) {
            Button row = new Button(this);
            row.setText(device.deviceName == null || device.deviceName.isEmpty() ? "ELE device" : device.deviceName);
            row.setOnClickListener(v -> connect(device));
            peerList.addView(row);
        }
        for (Map.Entry<String, NsdServiceInfo> entry : lanPeers.entrySet()) {
            NsdServiceInfo peer = entry.getValue();
            Button row = new Button(this);
            row.setText("LAN / wired · " + entry.getKey());
            row.setOnClickListener(v -> connectSocket(peer.getHost(), peer.getPort(), "LAN / wired"));
            peerList.addView(row);
        }
    }

    /** Starts a bounded TCP listener advertised on local Wi-Fi, Ethernet, or USB-Ethernet. */
    private void startLanTransport() {
        try {
            transportServer = new ServerSocket(SERVICE_PORT);
        } catch (IOException e) {
            setStatus("Could not open the local sharing service port.");
            return;
        }
        acceptWorker.execute(() -> {
            while (!cancelled && transportServer != null && !transportServer.isClosed()) {
                try {
                    Socket socket = transportServer.accept();
                    try {
                        worker.execute(() -> establishPairing(socket, AndroidTlsPairing.Role.SERVER, false));
                    } catch (RejectedExecutionException busy) {
                        try { socket.close(); } catch (IOException ignored) { }
                    }
                } catch (IOException e) {
                    if (!cancelled) runOnUiThread(() -> setStatus("Local network listener stopped."));
                    break;
                }
            }
        });
        if (nsdManager == null) return;
        NsdServiceInfo service = new NsdServiceInfo();
        ownServiceName = "ELE Share " + Integer.toHexString(android.os.Process.myPid());
        service.setServiceName(ownServiceName);
        service.setServiceType(SERVICE_TYPE);
        service.setPort(SERVICE_PORT);
        registrationListener = new NsdManager.RegistrationListener() {
            @Override public void onServiceRegistered(NsdServiceInfo registered) {
                ownServiceName = registered.getServiceName();
                runOnUiThread(() -> setStatus("Ready on nearby Wi-Fi and wired networks. Searching for peers…"));
            }
            @Override public void onRegistrationFailed(NsdServiceInfo info, int error) { }
            @Override public void onServiceUnregistered(NsdServiceInfo info) { }
            @Override public void onUnregistrationFailed(NsdServiceInfo info, int error) { }
        };
        nsdManager.registerService(service, NsdManager.PROTOCOL_DNS_SD, registrationListener);
    }

    @SuppressWarnings("deprecation")
    private void startLanDiscovery() {
        if (nsdManager == null) return;
        discoveryListener = new NsdManager.DiscoveryListener() {
            @Override public void onDiscoveryStarted(String type) { }
            @Override public void onServiceFound(NsdServiceInfo info) {
                if (!SERVICE_TYPE.equals(info.getServiceType())) return;
                nsdManager.resolveService(info, new NsdManager.ResolveListener() {
                    @Override public void onResolveFailed(NsdServiceInfo failed, int error) { }
                    @Override public void onServiceResolved(NsdServiceInfo resolved) {
                        if (resolved.getHost() == null || resolved.getPort() <= 0
                                || resolved.getServiceName().equals(ownServiceName)) return;
                        runOnUiThread(() -> {
                            lanPeers.put(resolved.getServiceName(), resolved);
                            renderLanPeers();
                        });
                    }
                });
            }
            @Override public void onServiceLost(NsdServiceInfo info) {
                runOnUiThread(() -> { lanPeers.remove(info.getServiceName()); renderLanPeers(); });
            }
            @Override public void onDiscoveryStopped(String type) { }
            @Override public void onStartDiscoveryFailed(String type, int error) {
                nsdManager.stopServiceDiscovery(this);
            }
            @Override public void onStopDiscoveryFailed(String type, int error) {
                nsdManager.stopServiceDiscovery(this);
            }
        };
        nsdManager.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, discoveryListener);
    }

    private void connectSocket(InetAddress address, int port) {
        connectSocket(address, port, "local network");
    }

    private void connectSocket(InetAddress address, int port, String transport) {
        if (sessionStarting || pairing != null) return;
        sessionStarting = true;
        transportName = transport;
        setStatus("Connecting over " + transport + "…");
        worker.execute(() -> {
            Socket socket = new Socket();
            try {
                socket.connect(new InetSocketAddress(address, port), 10_000);
                establishPairing(socket, AndroidTlsPairing.Role.CLIENT, true);
            } catch (IOException e) {
                try { socket.close(); } catch (IOException ignored) { }
                runOnUiThread(() -> { sessionStarting = false; setStatus("Local network connection failed: " + safeMessage(e)); });
            }
        });
    }

    private void establishPairing(Socket socket, AndroidTlsPairing.Role role, boolean reserved) {
        if ((!reserved && sessionStarting) || pairing != null) {
            try { socket.close(); } catch (IOException ignored) { }
            return;
        }
        sessionStarting = true;
        runOnUiThread(() -> setStatus("Secure pairing is starting over " + transportName + "…"));
        try {
            AndroidTlsPairing established = AndroidTlsPairing.establish(socket, role);
            pairing = established;
            runOnUiThread(() -> showPairConfirmation(established));
        } catch (IOException | GeneralSecurityException | RuntimeException e) {
            try { socket.close(); } catch (IOException ignored) { }
            sessionStarting = false;
            runOnUiThread(() -> setStatus("Secure pairing failed: " + safeMessage(e)));
        }
    }

    private void connect(WifiP2pDevice device) {
        WifiP2pConfig config = new WifiP2pConfig();
        config.deviceAddress = device.deviceAddress;
        setStatus("Requesting connection to " + device.deviceName + "…");
        manager.connect(channel, config, new WifiP2pManager.ActionListener() {
            @Override public void onSuccess() { setStatus("Connection requested. Accept the request on the other device."); }
            @Override public void onFailure(int reason) { setStatus("Could not connect to peer (" + reason + ")."); }
        });
    }

    private void establishDataChannel(WifiP2pManager.ConnectionInfo info) {
        if (!info.groupFormed) return;
        if (info.isGroupOwner) {
            transportName = "Wi-Fi Direct";
            setStatus("Connected over Wi-Fi Direct. Waiting for the peer to open the secure channel…");
        } else {
            connectSocket(info.groupOwnerAddress, SERVICE_PORT, "Wi-Fi Direct");
        }
    }

    private void showPairConfirmation(AndroidTlsPairing established) {
        String direction = offeredUris.isEmpty() ? "Receive" : "Send";
        String action = offeredUris.isEmpty() && destinationTree == null
                ? "Choose folder" : "Codes match — " + direction;
        new AlertDialog.Builder(this)
                .setTitle("Confirm nearby device")
                .setMessage("Compare this six-digit code on both devices:\n\n" + established.getSasCode()
                        + "\n\nPeer fingerprint (SHA-256):\n" + established.getPeerFingerprintSha256()
                        + "\n\nOnly continue if both codes match and you selected the intended device.")
                .setPositiveButton(action, (dialog, which) -> {
                    if (offeredUris.isEmpty() && destinationTree == null) {
                        closePairing();
                        chooseDestination();
                        return;
                    }
                    try {
                        established.confirmPeer(established.getSasCode());
                        if (offeredUris.isEmpty()) {
                            startReceive(established);
                        } else {
                            startSend(established);
                        }
                    } catch (GeneralSecurityException | IOException | RuntimeException e) {
                        closePairing();
                        setStatus("Could not start transfer: " + safeMessage(e));
                    }
                })
                .setNegativeButton("Cancel", (dialog, which) -> closePairing())
                .setOnCancelListener(dialog -> closePairing())
                .show();
    }

    private void startSend(AndroidTlsPairing established) throws IOException {
        List<TransferProtocol.FileSource> sources = new ArrayList<>();
        for (Uri uri : offeredUris) {
            String name = displayName(uri);
            long size = contentSize(uri);
            if (size < 0) throw new IOException("File size is unavailable for " + name);
            sources.add(new TransferProtocol.FileSource(name, size,
                    () -> getContentResolver().openInputStream(uri)));
        }
        setStatus("Sending " + sources.size() + " item(s) over authenticated " + transportName + "…");
        long totalBytes = 0;
        try {
            for (TransferProtocol.FileSource source : sources) totalBytes = Math.addExact(totalBytes, source.size);
        } catch (ArithmeticException e) {
            throw new IOException("Total transfer size is too large", e);
        }
        long transferSize = totalBytes;
        worker.execute(() -> runProtocol(established, (input, output) ->
                TransferProtocol.send(output, sources, () -> cancelled,
                        new TransferProtocol.Limits(128, Long.MAX_VALUE, 1024 * 1024, 4096)), true, transferSize));
    }

    private void startReceive(AndroidTlsPairing established) {
        receivedBytes = 0;
        setStatus("Receiving into the selected folder over authenticated " + transportName + "…");
        worker.execute(() -> runProtocol(established, (input, output) -> {
            TransferProtocol.ReceiveResult result = TransferProtocol.receive(input, this::createSink,
                    () -> cancelled, new TransferProtocol.Limits(128, Long.MAX_VALUE, 1024 * 1024, 4096));
            receivedBytes = result.totalBytes;
        }, false, 0));
    }

    private volatile long receivedBytes;
    private void runProtocol(AndroidTlsPairing established, AndroidTlsPairing.TransferProtocol protocol,
                             boolean sending, long sentBytes) {
        long startedAt = System.nanoTime();
        try {
            established.runTransferProtocol(protocol);
            long bytes = sending ? sentBytes : receivedBytes;
            double seconds = Math.max(0.001, (System.nanoTime() - startedAt) / 1_000_000_000.0);
            double megabytesPerSecond = bytes / seconds / (1024.0 * 1024.0);
            runOnUiThread(() -> setStatus((sending ? "Transfer sent" : "Transfer received and verified")
                    + ": " + String.format(java.util.Locale.US, "%.1f MiB/s", megabytesPerSecond)
                    + " measured payload throughput."));
        } catch (IOException | RuntimeException e) {
            runOnUiThread(() -> setStatus("Transfer stopped: " + safeMessage(e)));
        } finally {
            pairing = null;
            sessionStarting = false;
        }
    }

    private TransferProtocol.FileSink createSink(TransferProtocol.FileMetadata metadata) throws IOException {
        if (destinationTree == null) throw new IOException("Choose a receive folder first");
        String stagingName = ".ele-share-pending-" + UUID.randomUUID();
        Uri document = DocumentsContract.createDocument(getContentResolver(), destinationTree,
                "application/octet-stream", stagingName);
        if (document == null) throw new IOException("Could not create destination file");
        OutputStream output;
        try { output = getContentResolver().openOutputStream(document, "w"); }
        catch (RuntimeException e) {
            try { DocumentsContract.deleteDocument(getContentResolver(), document); }
            catch (Exception ignored) { }
            throw new IOException("Could not open destination file", e);
        }
        if (output == null) {
            try { DocumentsContract.deleteDocument(getContentResolver(), document); }
            catch (Exception ignored) { }
            throw new IOException("Could not open destination file");
        }
        Uri[] stagedDocument = {document};
        return new TransferProtocol.FileSink() {
            private boolean done;
            @Override public OutputStream outputStream() { return output; }
            @Override public void commit() throws IOException {
                if (done) return;
                output.close();
                try {
                    Uri visible = DocumentsContract.renameDocument(
                            getContentResolver(), stagedDocument[0], metadata.name);
                    if (visible == null) throw new IOException("Storage provider cannot publish the verified file");
                    stagedDocument[0] = visible;
                    done = true;
                } catch (RuntimeException e) {
                    throw new IOException("Could not publish the verified file", e);
                }
            }
            @Override public void abort() throws IOException {
                if (done) return;
                done = true;
                try { output.close(); }
                finally {
                    try { DocumentsContract.deleteDocument(getContentResolver(), stagedDocument[0]); }
                    catch (Exception e) { throw new IOException("Could not remove partial destination", e); }
                }
            }
        };
    }

    private String displayName(Uri uri) throws IOException {
        try (Cursor cursor = getContentResolver().query(uri,
                new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                String name = cursor.getString(0);
                if (name != null && !name.isEmpty()) return name;
            }
        } catch (RuntimeException e) { throw new IOException("Could not read file name", e); }
        throw new IOException("File name is unavailable");
    }

    private long contentSize(Uri uri) throws IOException {
        try (Cursor cursor = getContentResolver().query(uri,
                new String[] {OpenableColumns.SIZE}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) return cursor.getLong(0);
        } catch (RuntimeException e) { throw new IOException("Could not read file size", e); }
        return -1;
    }

    private void closePairing() {
        AndroidTlsPairing old = pairing;
        pairing = null;
        sessionStarting = false;
        if (old != null) old.close();
    }

    private void setStatus(String text) { if (status != null) status.setText(text); }
    private static String safeMessage(Throwable e) {
        String message = e.getMessage();
        return message == null ? e.getClass().getSimpleName() : message;
    }

    @Override protected void onDestroy() {
        cancelled = true;
        if (transportServer != null) try { transportServer.close(); } catch (IOException ignored) { }
        if (nsdManager != null && discoveryListener != null) {
            try { nsdManager.stopServiceDiscovery(discoveryListener); } catch (IllegalArgumentException ignored) { }
        }
        if (nsdManager != null && registrationListener != null) {
            try { nsdManager.unregisterService(registrationListener); } catch (IllegalArgumentException ignored) { }
        }
        closePairing();
        worker.shutdownNow();
        acceptWorker.shutdownNow();
        super.onDestroy();
    }

    private final class P2pReceiver extends BroadcastReceiver {
        @Override public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION.equals(action)) {
                if (manager != null && channel != null) manager.requestPeers(channel, ShareActivity.this::showPeers);
            } else if (WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION.equals(action)) {
                if (manager != null && channel != null) manager.requestConnectionInfo(channel, ShareActivity.this::establishDataChannel);
            } else if (WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION.equals(action)) {
                int state = intent.getIntExtra(WifiP2pManager.EXTRA_WIFI_STATE, -1);
                if (state != WifiP2pManager.WIFI_P2P_STATE_ENABLED) setStatus("Enable Wi-Fi for the direct peer transport.");
            }
        }
    }
}
