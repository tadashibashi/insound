#include "StreamSource.h"

#include "AudioDecoder.h"
#include "Error.h"
#include "lib.h"
#if defined(INSOUND_BACKEND_SDL2)
#   include <SDL_audio.h>
#endif

#include <iostream>

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
        Impl() : decoder(), converter(), looping(), isOneShot()
        {

        }

        AlignedVector<uint8_t, 16> buffer;
        AudioDecoder *decoder;

#if defined(INSOUND_BACKEND_SDL2)
        SDL_AudioStream *converter; ///< convert audio to target format
#endif
        bool looping, isOneShot;
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
        close();
        delete m;
    }

    bool StreamSource::isOpen() const
    {
        return m->decoder != nullptr;
    }

    bool StreamSource::open(const fs::path &filepath)
    {
        if (!filepath.has_extension())
        {
            INSOUND_PUSH_ERROR(Result::InvalidArg,
                "`filepath` must have an extension to infer its filetype");
            return false;
        }

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
        AudioDecoder *decoder = AudioDecoder::create(filepath, targetSpec);
        if (!decoder)
        {
            return false;
        }

        if (!decoder->setLooping(m->looping))
        {
            delete decoder;
            return false;
        }

        AudioSpec sourceSpec;
        if (!decoder->getSpec(&sourceSpec))
        {
            delete decoder;
            return false;
        }

#if defined(INSOUND_BACKEND_SDL2)
        // Init audio converter
        auto converter = SDL_NewAudioStream(
            sourceSpec.format.flags(), sourceSpec.channels, sourceSpec.freq,
            targetSpec.format.flags(), targetSpec.channels, targetSpec.freq);
        if (!converter)
        {
            INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
            delete decoder;
            return false;
        }

        // Done, commit changes
        // Clean up preexisting decoder/converter
        if (m->converter)
        {
            SDL_FreeAudioStream(m->converter);
        }
#endif
        delete m->decoder;
        m->decoder = decoder;
        m->converter = converter;
        m->buffer.resize(bufferSize, 0);
        return true;
    }

    void StreamSource::close()
    {
        if (m)
        {
            if (m->decoder)
            {
                delete m->decoder;
                m->decoder = nullptr;
            }

            if (m->converter)
            {
#if defined(INSOUND_BACKEND_SDL2)
                SDL_FreeAudioStream(m->converter);
                m->converter = nullptr;
#endif
            }
        }
    }

    int StreamSource::bytesAvailable() const
    {
        if (!m->converter)
            return 0;
#if defined(INSOUND_BACKEND_SDL2)
        return SDL_AudioStreamAvailable(m->converter);
#endif
    }

    bool StreamSource::isReady(bool *outReady) const
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

        if (m->buffer.size() != length)
            m->buffer.resize(length);

        // TODO: Put this in another thread
        queueNextBuffer();

#if defined(INSOUND_BACKEND_SDL2)
        // Retrieve when there is enough audio ready
        if (SDL_AudioStreamAvailable(m->converter) < length)
        {
            std::memset(output, 0, length);
            if (m->isOneShot) // if sound ended and nothing to read, close the source
            {
                bool ended;
                if (m->decoder->isEnded(&ended) && ended)
                {
                    close();
                    return length;
                }
            }
            return length;
        }

        const auto read = SDL_AudioStreamGet(m->converter, output, length);
        if (read <= 0)
        {
            INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
            return 0;
        }

        return read;
#endif
    }

    bool StreamSource::getLooping(bool *outLooping) const
    {
        INIT_GUARD();
        return m->decoder->getLooping(outLooping);
    }

    bool StreamSource::getPosition(TimeUnit units, double *outPosition) const
    {
        INIT_GUARD();
        return m->decoder->getPosition(units, outPosition);
    }

    bool StreamSource::setPosition(TimeUnit units, uint64_t position)
    {
        INIT_GUARD();
        return m->decoder->setPosition(units, position);
    }

    bool StreamSource::setLooping(bool looping)
    {
        INIT_GUARD();
        return m->decoder->setLooping(looping);
    }


    bool StreamSource::init(class Engine *engine, const fs::path &filepath,
        uint32_t parentClock, bool paused, bool isLooping, bool isOneShot)
    {
        if (!Source::init(engine, parentClock, paused))
            return false;
        m->looping = isLooping;
        m->isOneShot = isOneShot;
        if (!open(filepath))
        {
            return false;
        }

        return true;
    }

    bool StreamSource::release()
    {
        close();
        return true;
    }

    void StreamSource::queueNextBuffer()
    {
        if (!m->decoder)
            return;

#if defined(INSOUND_BACKEND_SDL2)
        const auto bufSize = m->buffer.size();
        if (SDL_AudioStreamAvailable(m->converter) < bufSize * 4)
        {
            // Reading probably needs to happen inside another thread since
            // reading from disk is slow.
            auto read = m->decoder->readBytes(
                static_cast<int>(bufSize), m->buffer.data());
            if (read > 0)
                SDL_AudioStreamPut(m->converter, m->buffer.data(), read);
        }
#endif
    }

}
