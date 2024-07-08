#include "StreamSource.h"

#include "AudioDecoder.h"
#include "Error.h"
#include "lib.h"
#include "logging.h"
#include "path.h"

///!!! TODO: Move this into the DataConverter class
#if defined(INSOUND_BACKEND_SDL2)
#include <SDL2/SDL_audio.h>
#else
#include <insound/core/external/miniaudio.h>
#endif

namespace insound {
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

        AlignedVector<uint8_t, 16> buffer{};
        AudioDecoder decoder{};
        bool looping{}, isOneShot{};

#if defined(INSOUND_BACKEND_SDL2)
        SDL_AudioStream *converter{};
#else
        ma_data_converter *converter{};
        int bytesAvailable{};
#endif
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

    bool StreamSource::open(const std::string &filepath, const bool inMemory)
    {
        AudioSpec targetSpec;
        if (!m_engine->getSpec(&targetSpec))
        {
            return false;
        }

        uint32_t bufferSize;
        if (!m_engine->getBufferSize(&bufferSize))
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

        uint64_t pcmLength;
        if (!decoder.getPCMFrameLength(&pcmLength))
        {
            return false;
        }

        AudioSpec sourceSpec;
        if (!decoder.getSpec(&sourceSpec))
        {
            return false;
        }

        m->buffer.resize(bufferSize, 0);
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

    int StreamSource::bytesAvailable() const
    {
        INIT_GUARD();
        return m->bytesAvailable;
    }

    bool StreamSource::isReady(bool *outReady) const // TODO: not necessarily correct across all platforms...
    {
        INIT_GUARD();

        uint32_t bufferSize;
        if (!m_engine->getBufferSize(&bufferSize))
            return false;
        if (outReady)
        {
            *outReady = bytesAvailable() >= bufferSize;
        }

        return true;
    }

    int StreamSource::readImpl(uint8_t *output, int length)
    {
        if (!isOpen())
        {
            std::memset(output, 0, length);
            return length;
        }

#if defined(INSOUND_BACKEND_SDL2)
        queueNextBuffer();

        if (int bytesAvailable = SDL_AudioStreamAvailable(m->converter); bytesAvailable < length)
        {
            std::memset(output, 0, length);
            if (m->isOneShot) // if sound ended and nothing to read, close the source
            {
                bool ended;
                if (m->decoder->isEnded(&ended) && ended)
                {
                    if (SDL_AudioStreamFlush(m->converter) != 0)
                        INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());

                    if (bytesAvailable = SDL_AudioStreamAvailable(m->converter) > 0;
                        bytesAvailable > 0)
                    {
                        if (SDL_AudioStreamGet(m->converter, output, bytesAvailable) != 0)
                            INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
                    }
                    close();
                    return length;
                }
            }
            return length;
        }

        if (SDL_AudioStreamGet(m->converter, output, length) <= 0)
        {
            INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
            std::memset(output, 0, length);
        }

        return length;
#else
        AudioSpec targetSpec;
        m->decoder.getTargetSpec(&targetSpec);

        const int outK = static_cast<int>(targetSpec.bytesPerFrame());
        const uint64_t outputFramesRequested = length / outK;

        auto framesRead = m->decoder.readFrames(outputFramesRequested, output);

        if (framesRead < 0)
        {
            // error occurred
            close();
        }

        if (m->isOneShot && !m->looping)
        {
            bool ended;
            if (m->decoder.isEnded(&ended) && ended)
            {
                std::memset(output, 0, length);
                close();
            }
        }

        return length;
#endif
    }

    bool StreamSource::getLooping(bool *outLooping) const
    {
        INIT_GUARD();
        return m->decoder.getLooping(outLooping);
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

    bool StreamSource::setLooping(bool looping)
    {
        INIT_GUARD();
        return m->decoder.setLooping(looping);
    }


    bool StreamSource::init(class Engine *engine, const std::string &filepath,
        uint32_t parentClock, bool paused, bool isLooping, bool isOneShot, bool inMemory)
    {
        if (!Source::init(engine, parentClock, paused))
            return false;
        m->looping = isLooping;
        m->isOneShot = isOneShot;
        if (!open(filepath, inMemory))
        {
            return false;
        }

        return true;
    }

    void StreamSource::queueNextBuffer()
    {
        if (!isOpen())
            return;
        const auto bufSize = static_cast<int>(m->buffer.size());
        if (bufSize == 0)
            return;

#if defined(INSOUND_BACKEND_SDL2)
        if (bytesAvailable() < bufSize * 4)
        {
            m->decoder->readBytes(bufSize, m->buffer.data());
            if (SDL_AudioStreamPut(m->converter, m->buffer.data(), bufSize) != 0)
            {
                INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
            }
        }
#else
        if (m->bytesAvailable < bufSize)
        {
            const auto bytesRead = m->decoder.readBytes(bufSize - m->bytesAvailable, m->buffer.data() + m->bytesAvailable);
            if (bytesRead < 0)
                close();
            m->bytesAvailable += bytesRead;
        }
#endif
    }

}
