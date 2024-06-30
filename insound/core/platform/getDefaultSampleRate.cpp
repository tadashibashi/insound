#include "getDefaultSampleRate.h"
#include "../Error.h"
#include <insound/core/lib.h>

#if INSOUND_TARGET_EMSCRIPTEN
#include <emscripten/emscripten.h>

// WebAudio backend
int insound::getDefaultSampleRate() {
    return EM_ASM_INT({
        var AudioContext = window.AudioContext || window.webkitAudioContext;
        if (AudioContext)
        {
            var context = new AudioContext();
            var sampleRate = context.sampleRate;
            context.close();
            return sampleRate;
        }

        return -1;
    });
}
#elif INSOUND_TARGET_ANDROID
#include <insound/core/platform/AndroidNative.h>
int insound::getDefaultSampleRate()
{
    return getAndroidDefaultSampleRate();
}

#else

#ifdef INSOUND_BACKEND_SDL2
#include <SDL2/SDL_audio.h>

// SDL backend
int insound::getDefaultSampleRate()
{
    SDL_AudioSpec defaultSpec;
    if (SDL_GetDefaultAudioInfo(nullptr, &defaultSpec, SDL_FALSE) != 0)
    {
        INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
        return -1;
    }

    return defaultSpec.freq;
}
#elif INSOUND_BACKEND_MINIAUDIO
#include <miniaudio.h>

#endif

#endif
