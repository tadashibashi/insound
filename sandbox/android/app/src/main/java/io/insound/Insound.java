package io.insound;

import android.app.Activity;
import android.content.res.AssetManager;

public class Insound
{
    public static void init(Activity activity)
    {
        nativeInit(activity);
        provideAssetManager(activity.getAssets());
    }

    public static void close()
    {
        nativeClose();
    }

    private static native void nativeInit(Activity activity);
    private static native void nativeClose();
    private static native void provideAssetManager(AssetManager mgr);
}
