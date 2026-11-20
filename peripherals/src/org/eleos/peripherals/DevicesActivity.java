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
import android.util.Size;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.*;
import java.util.Collections;
import java.util.ArrayList;

public final class DevicesActivity extends Activity implements SurfaceHolder.Callback {
    private final Handler main = new Handler(Looper.getMainLooper());
    private ConnectivityManager networks;
    private CameraManager cameras;
    private CameraDevice camera;
    private SurfaceView preview;
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
        if (caps == null) route.setText("No default data connection reported.");
        else if (caps.hasTransport(NetworkCapabilities.TRANSPORT_SATELLITE))
            route.setText("Android reports a satellite transport on the default data connection.");
        else route.setText("The default data connection does not report a satellite transport.");
    }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        networks = getSystemService(ConnectivityManager.class);
        cameras = getSystemService(CameraManager.class);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        route = new TextView(this);
        status = new TextView(this);
        status.setText("Connect an external camera in webcam mode.");
        Button start = new Button(this);
        start.setText("Preview external camera");
        start.setOnClickListener(v -> {
            if (checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED)
                openExternal();
            else requestPermissions(new String[]{Manifest.permission.CAMERA}, 1);
        });
        preview = new SurfaceView(this);
        preview.getHolder().addCallback(this);
        layout.addView(route);
        layout.addView(start);
        layout.addView(status);
        layout.addView(preview, new LinearLayout.LayoutParams(-1, 0, 1));
        setContentView(layout);
    }

    @Override public void onRequestPermissionsResult(int code, String[] names, int[] grants) {
        super.onRequestPermissionsResult(code, names, grants);
        if (code == 1 && grants.length > 0 && grants[0] == PackageManager.PERMISSION_GRANTED) {
            if (foreground) openExternal();
            else pendingPreview = true;
        }
        else status.setText("Camera access is needed for a preview.");
    }

    private void stopCamera() {
        generation++;
        if (camera != null) { camera.close(); camera = null; }
    }

    private void openExternal() {
        stopCamera();
        if (!foreground || !preview.getHolder().getSurface().isValid()) {
            status.setText("Preview surface unavailable. Try again when visible.");
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
                status.setText("No external preview camera exposed by this phone. Check webcam mode and USB host support.");
            } else if (ids.size() == 1) {
                openCameraId(ids.get(0));
            } else {
                String[] labels = new String[ids.size()];
                for (int i = 0; i < labels.length; i++) labels[i] = "External camera " + ids.get(i);
                new AlertDialog.Builder(this).setTitle("Choose camera")
                    .setItems(labels, (dialog, index) -> openCameraId(ids.get(index))).show();
            }
        } catch (CameraAccessException | RuntimeException error) {
            fail(generation, "Could not list external cameras.");
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
                fail(attempt, "Camera has no supported preview sizes.");
                return;
            }
            Size best = sizes[0];
            for (Size size : sizes) {
                if (Math.abs(area(size) - 921600L) < Math.abs(area(best) - 921600L)) best = size;
            }
            preview.getHolder().setFixedSize(best.getWidth(), best.getHeight());
            status.setText("Opening external camera…");
            cameras.openCamera(id, new CameraDevice.StateCallback() {
                @Override public void onOpened(CameraDevice device) {
                    if (attempt != generation) { device.close(); return; }
                    camera = device;
                    startSession(device, attempt);
                }
                @Override public void onDisconnected(CameraDevice device) {
                    device.close();
                    fail(attempt, "External camera disconnected.");
                }
                @Override public void onError(CameraDevice device, int error) {
                    device.close();
                    fail(attempt, "Camera unavailable (error " + error + ").");
                }
            }, main);

        } catch (CameraAccessException | RuntimeException error) {
            fail(attempt, "Could not open the external camera.");
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
                        status.setText("External camera preview started.");
                    } catch (CameraAccessException | RuntimeException error) {
                        fail(attempt, "External camera stream could not start.");
                    }
                }
                @Override public void onConfigureFailed(CameraCaptureSession session) {
                    session.close();
                    fail(attempt, "Camera rejected the preview configuration.");
                }
            }));
        } catch (CameraAccessException | RuntimeException error) {
            fail(attempt, "Could not configure the external camera.");
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
        status.setText("Preview stopped.");
        super.onPause();
    }
    @Override public void surfaceCreated(SurfaceHolder holder) {}
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}
    @Override public void surfaceDestroyed(SurfaceHolder holder) { stopCamera(); }
}
