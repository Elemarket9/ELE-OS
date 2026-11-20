package org.eleos.peripherals;

import android.content.Context;
import android.util.AttributeSet;
import android.view.SurfaceView;

/**
 * A preview surface whose height follows the selected stream's aspect ratio.
 * The height is derived from the width, so the video is not stretched and the
 * view keeps a usable size inside a scrolling layout at any font scale.
 */
public final class AspectSurfaceView extends SurfaceView {
    private int ratioWidth = 16;
    private int ratioHeight = 9;

    public AspectSurfaceView(Context context) { super(context); }
    public AspectSurfaceView(Context context, AttributeSet attrs) { super(context, attrs); }

    /** Sets the content aspect ratio. Non-positive values are ignored. */
    public void setAspectRatio(int width, int height) {
        if (width <= 0 || height <= 0) return;
        if (width == ratioWidth && height == ratioHeight) return;
        ratioWidth = width;
        ratioHeight = height;
        requestLayout();
    }

    @Override protected void onMeasure(int widthSpec, int heightSpec) {
        int width = MeasureSpec.getSize(widthSpec);
        int height = (int) ((long) width * ratioHeight / ratioWidth);
        setMeasuredDimension(width, resolveSize(height, heightSpec));
    }
}
