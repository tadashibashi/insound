#include "SoundBuffer.h"
#include "AudioSpec.h"
#include "io/loadAudio.h"

namespace insound {
    SoundBuffer::SoundBuffer() : m_bufferSize(), m_buffer(), m_spec()
    {
    }

    SoundBuffer::SoundBuffer(const std::string &filepath, const AudioSpec &targetSpec) :
        m_bufferSize(), m_buffer(), m_spec()
    {
        load(filepath, targetSpec);
    }

    SoundBuffer::~SoundBuffer()
    {
        unload();
    }

    SoundBuffer::SoundBuffer(SoundBuffer &&other) noexcept :
        m_bufferSize(other.m_bufferSize), m_buffer(other.m_buffer.load()), m_spec(other.m_spec)
    {
        other.m_spec = {};
        other.m_bufferSize = 0;
        other.m_buffer.store(nullptr);
    }

    bool SoundBuffer::load(const std::string &filepath, const AudioSpec &targetSpec)
    {
        uint8_t *buffer;
        uint32_t byteLength;
        if (!loadAudio(filepath, targetSpec, &buffer, &byteLength, nullptr))
        {
            return false;
        }

        emplace(buffer, byteLength, targetSpec);
        return true;
    }

    void SoundBuffer::unload()
    {
        if (isLoaded())
        {
            auto oldBuffer = m_buffer.load();
            while(!m_buffer.compare_exchange_weak(oldBuffer, nullptr)) { }

            m_bufferSize = 0;
            std::free(oldBuffer);
        }
    }

    void SoundBuffer::emplace(uint8_t *buffer, uint32_t bufferSize, const AudioSpec &spec)
    {
        std::swap(m_bufferSize, bufferSize);
        m_spec = spec;

        auto oldBuffer = m_buffer.load(std::memory_order_relaxed);
        while(!m_buffer.compare_exchange_weak(oldBuffer, buffer)) { }

        if (oldBuffer != nullptr)
        {
            std::free(oldBuffer);
        }
    }
}

