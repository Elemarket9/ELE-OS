package org.eleos.peripherals;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.pm.PackageManager;
import android.hardware.camera2.*;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.hardware.camera2.params.OutputConfiguration;
import android.hardware.camera2.params.SessionConfiguration;
import android.net.*;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.BidiFormatter;
import android.util.Size;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.widget.TextView;
import java.util.Collections;
import java.util.ArrayList;

public final class DevicesActivity extends Activity implements SurfaceHolder.Callback {
    private final Handler main = new Handler(Looper.getMainLooper());
    private ConnectivityManager networks;
    private CameraManager cameras;
    private CameraDevice camera;
    private AspectSurfaceView preview;
    private TextView route, status;
    private int generation;
    private boolean foreground;
    private boolean pendingPreview;

    private final ConnectivityManager.NetworkCallback changes = new ConnectivityManager.NetworkCallback() {
        @Override public void onCapabilitiesChanged(Network network, NetworkCapabilities caps) {
            refreshRoute();
        }
        @Override public void onLost(Network network) { refreshRoute(); }
    };

    private void refreshRoute() {
        Network active = networks.getActiveNetwork();
        NetworkCapabilities caps = networks.getNetworkCapabilities(active);
        if (caps == null) route.setText(R.string.route_none);
        else if (caps.hasTransport(NetworkCapabilities.TRANSPORT_SATELLITE))
            route.setText(R.string.route_satellite);
        else route.setText(R.string.route_not_satellite);
    }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        networks = getSystemService(ConnectivityManager.class);
        cameras = getSystemService(CameraManager.class);
        setContentView(R.layout.devices);
        route = findViewById(R.id.route);
        status = findViewById(R.id.status);
        preview = findViewById(R.id.preview);
        preview.getHolder().addCallback(this);
        findViewById(R.id.preview_button).setOnClickListener(v -> {
            if (checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED)
                openExternal();
            else requestPermissions(new String[]{Manifest.permission.CAMERA}, 1);
        });
    }

    @Override public void onRequestPermissionsResult(int code, String[] names, int[] grants) {
        super.onRequestPermissionsResult(code, names, grants);
        if (code == 1 && grants.length > 0 && grants[0] == PackageManager.PERMISSION_GRANTED) {
            if (foreground) openExternal();
            else pendingPreview = true;
        }
        else status.setText(R.string.permission_needed);
    }

    private void stopCamera() {
        generation++;
        if (camera != null) { camera.close(); camera = null; }
    }

    private void openExternal() {
        stopCamera();
        Surface surface = preview.getHolder().getSurface();
        if (!foreground || surface == null || !surface.isValid()) {
            status.setText(R.string.surface_unavailable);
            return;
        }
        try {
            ArrayList<String> ids = new ArrayList<>();
            for (String id : cameras.getCameraIdList()) {
                CameraCharacteristics info = cameras.getCameraCharacteristics(id);
                if (!Integer.valueOf(CameraCharacteristics.LENS_FACING_EXTERNAL)
                        .equals(info.get(CameraCharacteristics.LENS_FACING))) continue;
                ids.add(id);
            }
            if (ids.isEmpty()) {
                status.setText(R.string.no_external_camera);
            } else if (ids.size() == 1) {
                openCameraId(ids.get(0));
            } else {
                String[] labels = new String[ids.size()];
                BidiFormatter bidi = BidiFormatter.getInstance();
                for (int i = 0; i < labels.length; i++)
                    labels[i] = getString(R.string.external_camera_label, bidi.unicodeWrap(ids.get(i)));
                new AlertDialog.Builder(this).setTitle(R.string.choose_camera)
                    .setItems(labels, (dialog, index) -> openCameraId(ids.get(index))).show();
            }
        } catch (CameraAccessException | RuntimeException error) {
            fail(generation, getString(R.string.list_failed));
        }
    }

    private void openCameraId(String id) {
        if (!foreground) return;
        final int attempt = generation;
        try {
            CameraCharacteristics info = cameras.getCameraCharacteristics(id);
            StreamConfigurationMap map = info.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            Size[] sizes = map == null ? null : map.getOutputSizes(SurfaceHolder.class);
            if (sizes == null || sizes.length == 0) {
                fail(attempt, getString(R.string.no_preview_sizes));
                return;
            }
            Size best = sizes[0];
            for (Size size : sizes) {
                if (Math.abs(area(size) - 921600L) < Math.abs(area(best) - 921600L)) best = size;
            }
            preview.getHolder().setFixedSize(best.getWidth(), best.getHeight());
            preview.setAspectRatio(best.getWidth(), best.getHeight());
            status.setText(R.string.opening);
            cameras.openCamera(id, new CameraDevice.StateCallback() {
                @Override public void onOpened(CameraDevice device) {
                    if (attempt != generation) { device.close(); return; }
                    camera = device;
                    startSession(device, attempt);
                }
                @Override public void onDisconnected(CameraDevice device) {
                    device.close();
                    fail(attempt, getString(R.string.disconnected));
                }
                @Override public void onError(CameraDevice device, int error) {
                    device.close();
                    fail(attempt, getString(R.string.camera_error, error));
                }
            }, main);

        } catch (CameraAccessException | RuntimeException error) {
            fail(attempt, getString(R.string.open_failed));
        }
    }

    private static long area(Size size) { return (long) size.getWidth() * size.getHeight(); }

    private void fail(int attempt, String message) {
        if (attempt == generation) { stopCamera(); status.setText(message); }
    }

    private void startSession(CameraDevice device, int attempt) {
        Surface surface = preview.getHolder().getSurface();
        try {
            CaptureRequest.Builder request = device.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            request.addTarget(surface);
            device.createCaptureSession(new SessionConfiguration(SessionConfiguration.SESSION_REGULAR,
                    Collections.singletonList(new OutputConfiguration(surface)), getMainExecutor(),
                    new CameraCaptureSession.StateCallback() {
                @Override public void onConfigured(CameraCaptureSession session) {
                    if (attempt != generation) { session.close(); return; }
                    try {
                        session.setRepeatingRequest(request.build(), null, main);
                        status.setText(R.string.preview_started);
                    } catch (CameraAccessException | RuntimeException error) {
                        fail(attempt, getString(R.string.stream_failed));
                    }
                }
                @Override public void onConfigureFailed(CameraCaptureSession session) {
                    session.close();
                    fail(attempt, getString(R.string.config_rejected));
                }
            }));
        } catch (CameraAccessException | RuntimeException error) {
            fail(attempt, getString(R.string.configure_failed));
        }
    }

    @Override protected void onResume() {
        super.onResume();
        foreground = true;
        if (pendingPreview) { pendingPreview = false; openExternal(); }
        networks.registerDefaultNetworkCallback(changes, main);
        refreshRoute();
    }
    @Override protected void onPause() {
        foreground = false;
        networks.unregisterNetworkCallback(changes);
        stopCamera();
        status.setText(R.string.preview_stopped);
        super.onPause();
    }
    @Override public void surfaceCreated(SurfaceHolder holder) {}
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}
    @Override public void surfaceDestroyed(SurfaceHolder holder) { stopCamera(); }
}
