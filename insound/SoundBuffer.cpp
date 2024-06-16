#include "SoundBuffer.h"
#include "AudioSpec.h"

#include <SDL_audio.h>

#include "Error.h"
#include "io/loadAudio.h"

namespace insound {
    SoundBuffer::SoundBuffer() : m_bufferSize(), m_buffer(), m_spec()
    {
    }

    SoundBuffer::SoundBuffer(const fs::path &filepath, const AudioSpec &targetSpec) :
        m_bufferSize(), m_buffer(), m_spec()
    {
        load(filepath, targetSpec);
    }

    SoundBuffer::SoundBuffer(uint8_t *buffer, size_t bufferSize, const AudioSpec &spec) :
        m_bufferSize(bufferSize), m_buffer(buffer), m_spec(spec)
    {
    }

    SoundBuffer::~SoundBuffer()
    {
        unload();
    }

    SoundBuffer::SoundBuffer(SoundBuffer &&other) noexcept :
        m_bufferSize(other.m_bufferSize), m_buffer(other.m_buffer.load()), m_spec(other.m_spec)
    {
        other.m_buffer.store(nullptr);
        other.m_spec = {};
        other.m_bufferSize = 0;
    }

    bool SoundBuffer::load(const fs::path &filepath, const AudioSpec &targetSpec)
    {
        uint8_t *buffer;
        uint32_t byteLength;
        if (!loadAudio(filepath, targetSpec, &buffer, &byteLength))
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

    bool SoundBuffer::convert(const AudioSpec &newSpec)
    {
        uint8_t *convertedBuffer = nullptr;
        uint32_t newSize;
        const auto result = convertAudio(m_buffer, m_bufferSize, m_spec, newSpec, &convertedBuffer, &newSize);

        if (convertedBuffer) // buffer may have been reallocated on failure. todo: it may be better to make this function non-mutating on failure
        {
            std::swap(m_bufferSize, newSize);
            m_spec = newSpec;

            auto oldBuffer = m_buffer.load(std::memory_order_relaxed);
            while(!m_buffer.compare_exchange_weak(oldBuffer, convertedBuffer)) { }
        }

        return result;
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

