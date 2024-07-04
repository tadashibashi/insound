#include "StreamSource.h"

#include "AudioDecoder.h"
#include "Error.h"
#include "lib.h"

#if defined(INSOUND_BACKEND_SDL2)
#include <SDL2/SDL_audio.h>
#else
#include <insound/core/external/miniaudio.h>
#endif

#include <insound/core/logging.h>

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
        AudioDecoder *decoder{};
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
        return m != nullptr && m->decoder != nullptr;
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

        // Init audio converter
#if defined(INSOUND_BACKEND_SDL2)
        auto converter = SDL_NewAudioStream(
            sourceSpec.format.flags(), sourceSpec.channels, sourceSpec.freq,
            targetSpec.format.flags(), targetSpec.channels, targetSpec.freq);
        if (!converter)
        {
            INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
            return false;
        }

        if (m->converter)
            SDL_FreeAudioStream(converter);
        m->converter = converter;
#else
        auto converter = new ma_data_converter();

        const auto converterConfig = ma_data_converter_config_init(
            (ma_format)toMaFormat(sourceSpec.format), (ma_format)toMaFormat(targetSpec.format),
            sourceSpec.channels, targetSpec.channels,
            sourceSpec.freq, targetSpec.freq);

        if (ma_data_converter_init(&converterConfig, nullptr, converter) != MA_SUCCESS)
        {
            INSOUND_PUSH_ERROR(Result::MaErr, "Failed to init data converter");
            delete converter;
            return false;
        }

        // Clean up existing converter
        if (m->converter)
        {
            ma_data_converter_uninit(m->converter, nullptr);
        }
        m->bytesAvailable = 0;
#endif

        delete m->decoder;
        m->decoder = decoder;
        m->converter = converter;
        m->buffer.resize(bufferSize, 0);
        return true;
    }

    bool StreamSource::release()
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
#else
                ma_data_converter_uninit(m->converter, nullptr);
                m->bytesAvailable = 0;
#endif
                m->converter = nullptr;
            }
        }

        return true;
    }

    int StreamSource::bytesAvailable() const
    {
        if (!m || !m->converter)
            return 0;
#if defined(INSOUND_BACKEND_SDL2)
        return SDL_AudioStreamAvailable(m->converter);
#else
        return m->bytesAvailable;
#endif
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
        constexpr int outK = 2 * sizeof(float); // output frame -> byte constant
        ma_uint64 outputFramesRequested = length / outK;
        ma_uint64 inputFramesRequested;
        if (ma_data_converter_get_required_input_frame_count(m->converter, outputFramesRequested, &inputFramesRequested) != MA_SUCCESS)
        {
            INSOUND_PUSH_ERROR(Result::MaErr, "failed to calculate required input frames")
            return 0;
        }

        AudioSpec inSpec;
        m->decoder->getSpec(&inSpec);

        const int inK = inSpec.channels * (inSpec.format.bits() / CHAR_BIT);
        auto inputBytes = inputFramesRequested * inK;
        if (m->buffer.size() != inputBytes)
            m->buffer.resize(inputBytes, 0);

        // TODO: Put this in another thread
        queueNextBuffer();

        // Retrieve when there is enough audio ready
        if (m->bytesAvailable < inputBytes)
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

        ma_uint64 outBufferFrameSize = outputFramesRequested;
        ma_uint64 inFramesRead = inputFramesRequested;
        uint64_t totalInFramesRead = 0;

        while (totalInFramesRead < inputFramesRequested)
        {
            ma_uint64 inFramesLeft = inputFramesRequested - totalInFramesRead;
            if (ma_data_converter_process_pcm_frames(m->converter, m->buffer.data() + totalInFramesRead * inK, &inFramesLeft, output, &outBufferFrameSize) != MA_SUCCESS)
            {
                INSOUND_PUSH_ERROR(Result::MaErr, "failed to process pcm frames");
                return 0;
            }

            totalInFramesRead += inFramesLeft; // inFramesLeft contains frames consumed
        }

        m->bytesAvailable -= static_cast<int>(inputBytes);

        return length;
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

    void StreamSource::queueNextBuffer()
    {
        if (!m || !m->decoder)
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
            const auto bytesRead = m->decoder->readBytes(bufSize - m->bytesAvailable, m->buffer.data() + m->bytesAvailable);
            m->bytesAvailable += bytesRead;
        }
#endif
    }

}
