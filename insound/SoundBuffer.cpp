#include "SoundBuffer.h"
#include "AudioSpec.h"

#include <SDL_audio.h>

#include "Error.h"
#include "io/loadAudio.h"

namespace insound {
    SoundBuffer::SoundBuffer() : m_bufferSize(), m_buffer(), m_spec()
    {
    }

    SoundBuffer::SoundBuffer(const fs::path &filepath, const AudioSpec &targetSpec) : m_bufferSize(), m_buffer(),
        m_spec()
    {
        load(filepath, targetSpec);
    }

    SoundBuffer::~SoundBuffer()
    {
        unload();
    }

    bool SoundBuffer::load(const fs::path &filepath, const AudioSpec &targetSpec)
    {
        uint8_t *buffer;
        uint32_t byteLength;
        if (!loadAudio(filepath, targetSpec, &buffer, &byteLength))
        {
            return false;
        }

        if (m_buffer)
        {
            free(m_buffer);
        }

        m_buffer = buffer;
        m_bufferSize = byteLength;
        m_spec = targetSpec;
        return true;
    }

    void SoundBuffer::unload()
    {
        if (isLoaded())
        {
            free(m_buffer);
            m_buffer = nullptr;
            m_bufferSize = 0;
        }
    }
}

