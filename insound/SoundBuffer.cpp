#include "SoundBuffer.h"
#include "AudioSpec.h"

#include <SDL_audio.h>

#include "Error.h"

namespace insound {
    SoundBuffer::SoundBuffer() : m_byteLength(), m_buffer(), m_freq(0), m_spec()
    {
    }

    SoundBuffer::~SoundBuffer()
    {
        unload();
    }

    bool SoundBuffer::load(const fs::path &filepath, const AudioSpec &targetSpec)
    {
        SDL_AudioSpec spec;
        uint32_t length;
        uint8_t *buffer;
        if (!SDL_LoadWAV(filepath.c_str(), &spec, &buffer, &length))
        {
            pushError(Result::SdlErr, SDL_GetError());
            return false;
        }

        SDL_AudioCVT cvt;
        const auto cvtResult = SDL_BuildAudioCVT(&cvt,
            spec.format, spec.channels, spec.freq,
            targetSpec.format.flags(), targetSpec.channels, targetSpec.freq);

        if (cvt.needed == SDL_FALSE || cvtResult < 0)
        {
            pushError(Result::SdlErr, SDL_GetError());
            return false;
        }

        if (cvtResult == 0) // no conversion is needed, e.g. wav has same format as target
        {
            m_buffer = buffer;
            m_byteLength = length;
            return true;
        }

        // Convert audio
        // prepare cvt buffer resizing members
        cvt.len = static_cast<int>(length);
        cvt.buf = (uint8_t *)SDL_realloc(buffer, cvt.len * cvt.len_mult);

        if (SDL_ConvertAudio(&cvt) != 0)
        {
            pushError(Result::SdlErr, SDL_GetError());
            SDL_free(cvt.buf);
            return false;
        }

        // Success!
        m_buffer = cvt.buf;
        m_byteLength = cvt.len_cvt;

        m_spec = targetSpec;
        return true;
    }

    void SoundBuffer::unload()
    {
        if (isLoaded())
        {
            SDL_free(m_buffer);
            m_buffer = nullptr;
            m_byteLength = 0;
        }
    }
}

