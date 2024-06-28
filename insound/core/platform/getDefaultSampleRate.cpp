#include "getDefaultSampleRate.h"
#include "../Error.h"

#ifdef __EMSCRIPTEN__
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
#else
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
#endif
