package org.eleos.peripherals;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Shadows.shadowOf;

import android.Manifest;
import android.app.Application;
import android.os.Looper;
import android.text.Layout;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ScrollView;
import android.widget.TextView;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.GraphicsMode;

/**
 * Host-side layout checks with real text measurement (native graphics).
 * Robolectric is not a device: these tests do not cover camera hardware,
 * fonts shipped on a handset, or rendering by SurfaceFlinger.
 */
@RunWith(RobolectricTestRunner.class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = 36)
public final class LocalizationTest {
    private static final char RLO = '‮';

    private DevicesActivity launch() {
        DevicesActivity activity = Robolectric.buildActivity(DevicesActivity.class).setup().get();
        shadowOf(Looper.getMainLooper()).idle();
        return activity;
    }

    private static List<TextView> texts(DevicesActivity a) {
        return Arrays.asList(a.findViewById(R.id.route_heading), a.findViewById(R.id.route),
                a.findViewById(R.id.camera_heading), a.findViewById(R.id.status),
                a.findViewById(R.id.preview_button));
    }

    private static String[] routeStrings(DevicesActivity a) {
        return new String[] {a.getString(R.string.route_none), a.getString(R.string.route_satellite),
                a.getString(R.string.route_not_satellite)};
    }

    @Test @Config(qualifiers = "en")
    public void englishStringsComeFromResources() {
        DevicesActivity a = launch();
        assertEquals("External devices", a.getTitle().toString());
        assertEquals("Preview external camera",
                ((Button) a.findViewById(R.id.preview_button)).getText().toString());
        assertEquals("Connect an external camera in webcam mode.",
                ((TextView) a.findViewById(R.id.status)).getText().toString());
        String route = ((TextView) a.findViewById(R.id.route)).getText().toString();
        assertTrue(route, Arrays.asList(routeStrings(a)).contains(route));
        assertEquals(View.LAYOUT_DIRECTION_LTR, a.findViewById(R.id.root).getLayoutDirection());
        assertEquals("Camera unavailable (error 3).", a.getString(R.string.camera_error, 3));
    }

    @Test @Config(qualifiers = "ar-rXB-ldrtl")
    public void rtlPseudolocaleMirrorsLayout() {
        DevicesActivity a = launch();
        assertEquals(View.LAYOUT_DIRECTION_RTL, a.findViewById(R.id.root).getLayoutDirection());
        Button button = a.findViewById(R.id.preview_button);
        assertTrue("ar-XB text expected", button.getText().toString().indexOf(RLO) >= 0);
        for (TextView view : texts(a)) {
            assertEquals(View.LAYOUT_DIRECTION_RTL, view.getLayoutDirection());
            Layout layout = view.getLayout();
            assertNotNull(layout);
            assertEquals(Layout.DIR_RIGHT_TO_LEFT, layout.getParagraphDirection(0));
        }
        // A short heading starts at the right edge in RTL.
        TextView heading = a.findViewById(R.id.camera_heading);
        Layout layout = heading.getLayout();
        assertTrue(layout.getLineLeft(0) > 0);
        assertEquals(layout.getWidth(), layout.getLineRight(0), 1f);
        String route = ((TextView) a.findViewById(R.id.route)).getText().toString();
        assertTrue(route, Arrays.asList(routeStrings(a)).contains(route));
        assertTrue(route.indexOf(RLO) >= 0);
    }

    @Test @Config(qualifiers = "en")
    public void ltrHeadingStartsAtLeftEdge() {
        Layout layout = ((TextView) launch().findViewById(R.id.camera_heading)).getLayout();
        assertEquals(0f, layout.getLineLeft(0), 1f);
        assertTrue(layout.getLineRight(0) < layout.getWidth());
    }

    @Test @Config(qualifiers = "sw")
    public void untranslatedLocaleFallsBackToEnglish() {
        DevicesActivity a = launch();
        assertEquals("Preview external camera",
                ((Button) a.findViewById(R.id.preview_button)).getText().toString());
        assertEquals(View.LAYOUT_DIRECTION_LTR, a.findViewById(R.id.root).getLayoutDirection());
    }

    @Test @Config(qualifiers = "fa-ldrtl")
    public void untranslatedRtlLocaleMirrorsWithEnglishFallback() {
        DevicesActivity a = launch();
        assertEquals("Preview external camera",
                ((Button) a.findViewById(R.id.preview_button)).getText().toString());
        assertEquals(View.LAYOUT_DIRECTION_RTL, a.findViewById(R.id.root).getLayoutDirection());
        Layout layout = ((TextView) a.findViewById(R.id.camera_heading)).getLayout();
        assertEquals(layout.getWidth(), layout.getLineRight(0), 1f);
    }

    @Test @Config(qualifiers = "en-rXA-w320dp-h480dp-mdpi")
    public void longTextAtDoubleFontScaleIsNotClipped() {
        RuntimeEnvironment.setFontScale(2.0f);
        DevicesActivity a = launch();
        float density = a.getResources().getDisplayMetrics().density;
        assertEquals(2.0f, a.getResources().getConfiguration().fontScale, 0.001f);
        for (TextView view : texts(a)) {
            Layout layout = view.getLayout();
            assertNotNull(layout);
            String text = view.getText().toString();
            assertTrue("en-XA text expected: " + text, text.startsWith("[") && text.endsWith("]"));
            int available = view.getWidth() - view.getTotalPaddingLeft() - view.getTotalPaddingRight();
            for (int line = 0; line < layout.getLineCount(); line++) {
                assertEquals(text, 0, layout.getEllipsisCount(line));
                assertTrue(text, layout.getLineWidth(line) <= available + 1);
            }
            assertTrue(text, view.getHeight() >= layout.getHeight());
        }
        Button button = a.findViewById(R.id.preview_button);
        assertTrue(button.getHeight() >= Math.round(48 * density));
        assertTrue("long text wraps", button.getLayout().getLineCount() > 1
                || ((TextView) a.findViewById(R.id.status)).getLayout().getLineCount() > 1);

        // The preview keeps a 16:9 placeholder below the text; the page scrolls.
        View preview = a.findViewById(R.id.preview);
        assertTrue(preview.getWidth() > 0);
        assertEquals(preview.getWidth() * 9 / 16, preview.getHeight());
        ScrollView root = a.findViewById(R.id.root);
        View content = root.getChildAt(0);
        assertTrue(content.getHeight() > root.getHeight());
        assertTrue(root.canScrollVertically(1));
    }

    @Test @Config(qualifiers = "ar-rXB-ldrtl")
    public void cameraStatusMessagesAreLocalized() {
        Application app = RuntimeEnvironment.getApplication();
        shadowOf(app).grantPermissions(Manifest.permission.CAMERA);
        DevicesActivity a = launch();
        a.findViewById(R.id.preview_button).performClick();
        shadowOf(Looper.getMainLooper()).idle();
        String status = ((TextView) a.findViewById(R.id.status)).getText().toString();
        List<String> expected = new ArrayList<>();
        for (int id : new int[] {R.string.surface_unavailable, R.string.no_external_camera})
            expected.add(a.getString(id));
        assertTrue(status, expected.contains(status));
        assertNotEquals(-1, status.indexOf(RLO));
    }

    @Test @Config(qualifiers = "en")
    public void previewFollowsStreamAspectRatio() {
        AspectSurfaceView preview = launch().findViewById(R.id.preview);
        preview.setAspectRatio(4, 3);
        ((ViewGroup) preview.getParent()).requestLayout();
        shadowOf(Looper.getMainLooper()).idle();
        assertEquals(preview.getWidth() * 3 / 4, preview.getHeight());
        preview.setAspectRatio(0, 3);
        assertEquals(preview.getWidth() * 3 / 4, preview.getHeight());
    }
}
