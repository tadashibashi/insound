#include "StreamSource.h"

#include <iostream>

#include "AudioDecoder.h"
#include "Error.h"
#include "io/decodeGME.h"
#include "io/decodeMp3.h"
#include "io/decodeWAV.h"
#include "lib.h"

#include <SDL_audio.h>

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
        Impl() : decoder(), stream(), looping(), isOneShot()
        {

        }

        AlignedVector<uint8_t, 16> buffer;
        AudioDecoder *decoder;
        SDL_AudioStream *stream; ///< convert audio to target format
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
        AudioDecoder *decoder = AudioDecoder::create(filepath);
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

        // Init audio stream
        auto stream = SDL_NewAudioStream(
            sourceSpec.format.flags(), sourceSpec.channels, sourceSpec.freq,
            targetSpec.format.flags(), targetSpec.channels, targetSpec.freq);
        if (!stream)
        {
            INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
            delete decoder;
            return false;
        }

        // Done, commit changes
        // Clean up preexisting decoder/stream
        delete m->decoder;
        if (m->stream)
        {
            SDL_FreeAudioStream(m->stream);
        }

        m->decoder = decoder;
        m->stream = stream;
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

            if (m->stream)
            {
                SDL_FreeAudioStream(m->stream);
                m->stream = nullptr;
            }
        }
    }

    int StreamSource::bytesAvailable() const
    {
        if (!m->stream)
            return 0;
        return SDL_AudioStreamAvailable(m->stream);
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
        // TODO: Put this in another thread
        queueNextBuffer();

        // Retrieve when there is enough audio ready
        if (SDL_AudioStreamAvailable(m->stream) < length)
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

        const auto read = SDL_AudioStreamGet(m->stream, output, length);
        if (read <= 0)
        {
            INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
            return 0;
        }

        return read;
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
        const auto bufSize = m->buffer.size();
        if (SDL_AudioStreamAvailable(m->stream) < bufSize * 4)
        {
            // Reading probably needs to happen inside another thread since
            // reading from disk is slow.
            auto read = m->decoder->readBytes(
                static_cast<int>(bufSize), m->buffer.data());
            if (read > 0)
                SDL_AudioStreamPut(m->stream, m->buffer.data(), read);
        }
    }

}
