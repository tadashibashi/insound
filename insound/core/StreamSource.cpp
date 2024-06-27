#include "StreamSource.h"

#include <iostream>

#include "AudioDecoder.h"
#include "Error.h"
#include "io/decodeMp3.h"

#include <SDL_audio.h>

#include "io/decodeGME.h"

namespace insound {
#ifdef INSOUND_DEBUG
#define INIT_GUARD() do { if (!isOpen()) { \
    pushError(Result::DecoderNotInit, __FUNCTION__); \
    return false; \
} } while(0)
#else
#define INIT_GUARD()
#endif

    struct StreamSource::Impl {
        Impl() : decoder(), stream()
        {

        }

        AlignedVector<uint8_t, 16> buffer;
        AudioDecoder *decoder;
        SDL_AudioStream *stream; ///< convert audio to target format
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
            pushError(Result::InvalidArg,
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
        AudioDecoder *decoder = nullptr;
        const auto ext = filepath.extension().string();
        if (ext == ".mp3")
        {
#if INSOUND_DECODE_MP3
            decoder = new Mp3Decoder();
#else
            pushError(Result::NotSupported, "Mp3 decoding is not supported, "
                      "make sure to compile with INSOUND_DECODE_MP3 defined");
            return false;
#endif      // TODO: add the other decoder types here
        }
#if INSOUND_DECODE_GME
        else
        {
            decoder = new GmeDecoder(targetSpec.freq);
        }
#endif


        if (decoder == nullptr)
        {
            pushError(Result::NotSupported, "File extension is not supported");
            return false;
        }

        if (!decoder->open(filepath))
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
        std::cout << "SourceSpec: Bits " << (float)sourceSpec.format.bits() << ", isFloat " << sourceSpec.format.isFloat() << ", chans " << sourceSpec.channels << ", freq " << sourceSpec.freq << '\n';
        // Init audio stream
        auto stream = SDL_NewAudioStream(
            sourceSpec.format.flags(), sourceSpec.channels, sourceSpec.freq,
            targetSpec.format.flags(), targetSpec.channels, targetSpec.freq);
        if (!stream)
        {
            pushError(Result::SdlErr, SDL_GetError());
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

    int StreamSource::readImpl(uint8_t *output, int length)
    {
        // TODO: Put this in another thread
        queueNextBuffer();

        // Retrieve when there is enough audio ready
        if (SDL_AudioStreamAvailable(m->stream) < length)
        {
            std::memset(output, 0, length);
            return length;
        }

        const auto read = SDL_AudioStreamGet(m->stream, output, length);
        if (read <= 0)
        {
            pushError(Result::SdlErr, SDL_GetError());
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
