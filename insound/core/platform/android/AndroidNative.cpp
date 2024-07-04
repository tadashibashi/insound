#include "AndroidNative.h"

#ifdef __ANDROID__
#include <insound/core/Error.h>
#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

static jobject g_activity;
static AAssetManager *g_assets;
static int defaultSampleRate = 48000;
static int defaultFramesPerBuffer = 512;

extern "C" JNIEXPORT void JNICALL
Java_io_insound_Insound_nativeInit(JNIEnv *env, jclass clazz, jobject activity)
{
    g_activity = activity;
}

extern "C" JNIEXPORT void JNICALL
Java_io_insound_Insound_nativeClose(JNIEnv *env, jclass clazz)
{
    g_activity = nullptr;
    g_assets = nullptr;
}

extern "C" JNIEXPORT void JNICALL
Java_io_insound_Insound_provideAssetManager(JNIEnv *env, jclass clazz, jobject assetMgr)
{
    g_assets = AAssetManager_fromJava(env, assetMgr);
}

extern "C" JNIEXPORT void JNICALL
Java_io_insound_Insound_provideAudioDefaults(JNIEnv *env, jclass clazz, jint sampleRate, jint framesPerBuffer)
{
    defaultSampleRate = static_cast<int>(sampleRate);
    defaultFramesPerBuffer = static_cast<int>(framesPerBuffer);
}


namespace insound {

    static AAsset *openAssetImpl(const char *filename, int mode)
    {
        if (!g_assets)
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "The Insound Android Native object was not initialized. "
                                          "Pleaese make sure to call io.insound.Insound.ini(this) "
                                          "in an Activity.onCreate");
            return nullptr;
        }

        return AAssetManager_open(g_assets, filename, mode);
    }

    AAsset *openAsset(const char *filename)
    {
        return openAssetImpl(filename, AASSET_MODE_BUFFER);
    }

    AAsset *openAssetStream(const char *filename)
    {
        return openAssetImpl(filename, AASSET_MODE_STREAMING);
    }

    int getAndroidDefaultSampleRate()
    {
        return defaultSampleRate;
    }

    int getAndroidDefaultFramesPerBuffer()
    {
        return defaultFramesPerBuffer;
    }

}

#endif
