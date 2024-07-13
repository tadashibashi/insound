#include "StreamSource.h"

#include "AudioDecoder.h"
#include "Error.h"

#include "lib.h"

#include <insound/core/external/miniaudio.h>
#include <insound/core/io/openFile.h>

namespace insound {

/// Macro to ensure that the StreamSource is open in a StreamSource function
#ifdef INSOUND_DEBUG
#define INIT_GUARD() do { if (!isOpen()) { \
    INSOUND_PUSH_ERROR(Result::DecoderNotInit, __FUNCTION__); \
    return false; \
} } while(0)
#else
#define INIT_GUARD() INSOUND_NOOP
#endif

    struct StreamSource::Impl {
        Impl() = default;

        AudioDecoder decoder{};
        bool looping{}, isOneShot{};
        int bytesPerFrame{};
    };

    StreamSource::StreamSource() : m(new Impl)
    {

    }

    StreamSource::StreamSource(StreamSource &&other) noexcept :
        Source(std::move(other)), m(other.m)
    {
        other.m = nullptr;
    }


    StreamSource::~StreamSource()
    {
        delete m;
    }

    bool StreamSource::isOpen() const
    {
        return m != nullptr && m->decoder.isOpen();
    }

    bool StreamSource::openConstMem(const uint8_t *data, const size_t size)
    {
        AudioSpec targetSpec;
        if (!m_engine->getSpec(&targetSpec))
        {
            return false;
        }

        uint32_t bufferSize;
        AudioDecoder decoder;

        if (!decoder.openConstMem(data, size, targetSpec))
        {
            return false;
        }

        m->decoder = std::move(decoder);
        m->bytesPerFrame = static_cast<int>(targetSpec.bytesPerFrame());
        return true;
    }

    bool StreamSource::open(const std::string &filepath, const bool inMemory)
    {
        AudioSpec targetSpec;
        if (!m_engine->getSpec(&targetSpec))
        {
            return false;
        }

        // Init audio decoder by file type
        AudioDecoder decoder;
        if (!decoder.open(filepath, targetSpec, inMemory))
        {
            return false;
        }

        if (!decoder.setLooping(m->looping))
        {
            return false;
        }

        m->bytesPerFrame = static_cast<int>(targetSpec.bytesPerFrame());
        m->decoder = std::move(decoder);
        return true;
    }

    bool StreamSource::release()
    {
        if (m)
        {
            m->decoder.close();
        }

        return true;
    }

    int StreamSource::readImpl(uint8_t *output, int length)
    {
        // Init guard
        if (!isOpen())
        {
            std::memset(output, 0, length);
            return length;
        }

        // Read the frames!
        const auto framesToRead = length / m->bytesPerFrame;
        const auto framesRead = m->decoder.readFrames(framesToRead, output);

        // Error check
        if (framesRead < 0)
        {
            close();
            std::memset(output, 0, length);
            return length;
        }

        // Ensure any remaining frame is filled with silence
        if (framesRead < framesToRead)
        {
            auto bytesRead = framesRead * m->bytesPerFrame;
            std::memset(output + bytesRead, 0, length - bytesRead);
        }

        // Auto-release on end of oneshot
        bool looping;
        if (!m->decoder.getLooping(&looping))
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "failed to get looping flag");
            return 0; // shouldn't happen, but just in case
        }

        if (m->isOneShot && !looping)
        {
            bool ended;
            if (m->decoder.isEnded(&ended) && ended)
            {
                close();
            }
        }

        return length;
    }

    bool StreamSource::getLooping(bool *outLooping) const
    {
        INIT_GUARD();
        if (outLooping)
            *outLooping = m->looping;
        return true;
    }

    bool StreamSource::setLooping(bool looping)
    {
        INIT_GUARD();

        return m->decoder.setLooping(looping);
    }

    bool StreamSource::getPosition(TimeUnit units, double *outPosition) const
    {
        INIT_GUARD();
        return m->decoder.getPosition(units, outPosition);
    }

    bool StreamSource::setPosition(TimeUnit units, uint64_t position)
    {
        INIT_GUARD();
        return m->decoder.setPosition(units, position);
    }

    bool StreamSource::init(class Engine *engine, const std::string &filepath,
        uint32_t parentClock, bool paused, bool isLooping, bool isOneShot, bool inMemory)
    {
        if (!Source::init(engine, parentClock, paused))
            return false;
        m->isOneShot = isOneShot;
        if (!open(filepath, inMemory))
        {
            return false;
        }

        m->decoder.setLooping(isLooping);

        return true;
    }
}
