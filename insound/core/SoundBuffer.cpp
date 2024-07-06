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

    SoundBuffer::SoundBuffer(uint8_t *buffer, uint32_t bufferSize, const AudioSpec &spec, std::vector<Marker> &&markers) :
        m_bufferSize(bufferSize), m_buffer(buffer), m_spec(spec), m_markers()
    {
        m_markers.swap(markers);
    }

    SoundBuffer::~SoundBuffer()
    {
        unload();
    }

    SoundBuffer::SoundBuffer(SoundBuffer &&other) noexcept :
        m_bufferSize(other.m_bufferSize), m_buffer(other.m_buffer.load()), m_spec(other.m_spec), m_markers()
    {
        other.m_markers.swap(m_markers);
        other.m_spec = {};
        other.m_bufferSize = 0;
        other.m_buffer.store(nullptr);
    }

    bool SoundBuffer::load(const std::string &filepath, const AudioSpec &targetSpec)
    {
        uint8_t *buffer;
        uint32_t byteLength;
        std::map<uint32_t, Marker> markers;
        if (!loadAudio(filepath, targetSpec, &buffer, &byteLength, &markers))
        {
            return false;
        }

        // Collect markers
        m_markers.clear();
        m_markers.reserve(markers.size());
        for (auto &[id, marker] : markers)
        {
            m_markers.emplace_back(marker);
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

    void SoundBuffer::addMarker(const std::string &label, TimeUnit unit, uint64_t position)
    {
        m_markers.emplace_back(label, (uint32_t)std::round(insound::convert(position, unit, TimeUnit::PCM, m_spec)));
    }
}

