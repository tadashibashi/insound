#pragma once
#include "../AudioSpec.h"
#include "../Error.h"

#include <SDL2/SDL_audio.h>

#include <cstdint>
#include <filesystem>

namespace insound {

    // TODO: add ability to parse cue chunk into marker data, convert sample positions if necessary
    inline bool decodeWAV(const std::filesystem::path &path, AudioSpec *outSpec, uint8_t **outBuffer, uint32_t *outBufferSize)
    {
        SDL_AudioSpec sdlspec;
        uint32_t length;
        uint8_t *buffer;
        if (!SDL_LoadWAV(path.c_str(), &sdlspec, &buffer, &length))
        {
            pushError(Result::SdlErr, SDL_GetError());
            return false;
        }

        if (outSpec)
        {
            AudioSpec spec;
            spec.channels = sdlspec.channels;
            spec.format = SampleFormat(
                SDL_AUDIO_BITSIZE(sdlspec.format),
                SDL_AUDIO_ISFLOAT(sdlspec.format),
                SDL_AUDIO_ISBIGENDIAN(sdlspec.format),
                SDL_AUDIO_ISSIGNED(sdlspec.format));
            spec.freq = sdlspec.freq;

            *outSpec = spec;
        }

        if (outBuffer)
        {
            *outBuffer = buffer;
        }
        else
        {
            free(buffer);
        }

        if (outBufferSize)
        {
            *outBufferSize = length;
        }

        return true;
    }
}
