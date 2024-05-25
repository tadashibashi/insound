#include "SoundBuffer.h"
#include <SDL_audio.h>

namespace insound {
    SoundBuffer::SoundBuffer() : m_byteLength(), m_buffer(), m_freq(0), m_channels(0), m_format(0)
    {
    }

    SoundBuffer::~SoundBuffer()
    {
        unload();
    }

    bool SoundBuffer::load(const fs::path &filepath, const SDL_AudioSpec &targetSpec)
    {
        SDL_AudioSpec spec;
        uint32_t length;
        uint8_t *buffer;
        if (!SDL_LoadWAV(filepath.c_str(), &spec, &buffer, &length))
        {
            printf("SDL_LoadWAV_RW failed: %s\n", SDL_GetError());
            return false;
        }

        SDL_AudioCVT cvt;
        const auto cvtResult = SDL_BuildAudioCVT(&cvt,
            spec.format, spec.channels, spec.freq,
            targetSpec.format, targetSpec.channels, targetSpec.freq);

        if (cvt.needed == SDL_FALSE || cvtResult < 0)
        {
            printf("SDL_BuildAudioCVT failed: %s\n", SDL_GetError());
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
            printf("SDL_ConvertAudio failed: %s\n", SDL_GetError());
            SDL_free(cvt.buf);
            return false;
        }

        // Success!
        m_buffer = cvt.buf;
        m_byteLength = cvt.len_cvt;
        m_freq = targetSpec.freq;
        m_format = targetSpec.format;
        m_channels = targetSpec.channels;
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

    bool SoundBuffer::isInt() const
    {
        return SDL_AUDIO_ISINT(m_format);
    }

    bool SoundBuffer::isFloat() const
    {
        return SDL_AUDIO_ISFLOAT(m_format);
    }

    bool SoundBuffer::isSigned() const
    {
        return SDL_AUDIO_ISSIGNED(m_format);
    }

    bool SoundBuffer::isUnsigned() const
    {
        return SDL_AUDIO_ISUNSIGNED(m_format);
    }

    int SoundBuffer::samplerate() const
    {
        return m_freq;
    }

    int SoundBuffer::bitSize() const
    {
        return SDL_AUDIO_BITSIZE(m_format);
    }

    uint8_t SoundBuffer::channels() const
    {
        return m_channels;
    }



}

