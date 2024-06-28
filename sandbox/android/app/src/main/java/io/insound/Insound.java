package io.insound;

import android.app.Activity;
import android.content.Context;
import android.content.res.AssetManager;
import android.media.AudioManager;

public class Insound
{
    public static void init(Activity activity)
    {
        nativeInit(activity);
        provideAssetManager(activity.getAssets());

        AudioManager am = (AudioManager) activity.getSystemService(Context.AUDIO_SERVICE);

        // Get the default audio sample rate and buffer size
        String sampleRateStr = am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
        int sampleRate = Integer.parseInt(sampleRateStr);

        String framesPerBufferStr = am.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        int framesPerBuffer = Integer.parseInt(framesPerBufferStr);

        provideAudioDefaults(sampleRate == 0 ? 48000 : sampleRate, framesPerBuffer == 0 ? 256 : framesPerBuffer);
    }

    public static void close()
    {
        nativeClose();
    }

    private static native void nativeInit(Activity activity);
    private static native void nativeClose();
    private static native void provideAssetManager(AssetManager mgr);
    private static native void provideAudioDefaults(int sampleRate, int framesPerBuffer);
}
