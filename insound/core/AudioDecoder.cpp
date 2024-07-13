#include "AudioDecoder.h"

#include "Error.h"
#include "lib.h"
#include "io/Rstream.h"
#include "io/Rstreamable.h"

#include "external/miniaudio.h"
#include "external/miniaudio_decoder_backends.h"

#include <vector>

static std::vector<ma_decoding_backend_vtable *> customBackendVTables;
static bool initBackendVTables;

namespace insound {
#ifdef INSOUND_DEBUG
#define INIT_GUARD() do { if (!isOpen()) { \
        INSOUND_PUSH_ERROR(Result::DecoderNotInit, __FUNCTION__); \
        return false; \
    } } while(0)
#else
#define INIT_GUARD() INSOUND_NOOP
#endif

    struct AudioDecoder::Impl {
        AudioSpec spec{}; ///< audio spec of source stream
        AudioSpec targetSpec{};
        bool looping{};   ///< loop mode: when true the stream seeks back to the beginning automatically
        ma_decoder *decoder{};
        Rstream stream{};
        uint64_t pcmLength{UINT64_MAX};
    };

    AudioDecoder::AudioDecoder() : m(new Impl)
    {
        if (!initBackendVTables)
        {
            customBackendVTables = {
#ifdef INSOUND_DECODE_VORBIS
                &g_ma_decoding_backend_vtable_libvorbis,
#endif
            };
            initBackendVTables = true;
        }
    }

    AudioDecoder::AudioDecoder(AudioDecoder &&other) noexcept : m(other.m)
    {
        close();
        other.m = nullptr;
    }

    AudioDecoder::~AudioDecoder()
    {
        delete m;
    }

    AudioDecoder &AudioDecoder::operator=(AudioDecoder &&other) noexcept
    {
        close();
        delete m;
        m = other.m;
        other.m = nullptr;

        return *this;
    }

    /// Miniaudio decoder read callback making use of the Rstream abstraction
    static ma_result ma_decoder_on_read_rstream(
        ma_decoder *decoder,
        void *bufferOut,
        const size_t bytesToRead,
        size_t *outBytesRead)
    {
        const auto stream = static_cast<Rstreamable *>(decoder->pUserData);
        const auto bytesRead = stream->read(static_cast<uint8_t *>(bufferOut), static_cast<int64_t>(bytesToRead));
        if (bytesRead < 0)
        {
            *outBytesRead = 0;
            return MA_ERROR;
        }
        else if (bytesRead < bytesToRead)
        {
            *outBytesRead = bytesRead;
            return MA_AT_END;
        }

        *outBytesRead = static_cast<size_t>(bytesRead);
        return MA_SUCCESS;
    }

    /// Miniaudio decoder seek callback making use of the Rstream abstraction
    static ma_result ma_decoder_on_seek_rstream(
        ma_decoder *decoder,
        const int64_t offset,
        const ma_seek_origin origin)
    {
        const auto stream = static_cast<Rstreamable *>(decoder->pUserData);
        int64_t seekPosition = 0;
        switch(origin)
        {
            case ma_seek_origin_current:
                seekPosition = offset + stream->tell();
            break;
            case ma_seek_origin_start:
                seekPosition = offset;
            break;
            case ma_seek_origin_end:
                seekPosition = offset + stream->size();
            break;
            default:
                INSOUND_PUSH_ERROR(Result::MaErr, "invalid seek origin enum value");
            break;
        }

        return stream->seek(seekPosition) ? MA_SUCCESS : MA_BAD_SEEK;
    }

    bool AudioDecoder::openConstMem(const uint8_t *data, size_t dataSize, const AudioSpec &targetSpec)
    {
        if (!m->stream.openConstMem(data, dataSize))
        {
            return false;
        }

        return postOpen(targetSpec);
    }

    bool AudioDecoder::openMem(uint8_t *data, size_t dataSize, const AudioSpec &targetSpec, void(*deallocator)(void *data))
    {
        if (!m->stream.openMem(data, dataSize, deallocator))
        {
            return false;
        }

        return postOpen(targetSpec);
    }

    bool AudioDecoder::open(const std::string &filepath, const AudioSpec &targetSpec, const bool inMemory)
    {
        if (!m->stream.openFile(filepath, inMemory))
        {
            return false;
        }

        return postOpen(targetSpec);
    }

    bool AudioDecoder::postOpen(const AudioSpec &targetSpec)
    {
        const auto decoder = new ma_decoder;
        ma_decoder_config config = ma_decoder_config_init(
            (ma_format)toMaFormat(targetSpec.format),
            targetSpec.channels,
            targetSpec.freq);
        config.pCustomBackendUserData = nullptr;
        config.ppCustomBackendVTables = customBackendVTables.data();
        config.customBackendCount = static_cast<ma_uint32>(customBackendVTables.size());

        if (auto result = ma_decoder_init(
                ma_decoder_on_read_rstream,
                ma_decoder_on_seek_rstream,
                m->stream.stream(),
                &config,
                decoder);
            result != MA_SUCCESS)
        {
            INSOUND_PUSH_ERROR(Result::MaErr, ma_result_description(result));
            delete decoder;
            return false;
        }

        ma_format format;
        ma_uint32 channels;
        ma_uint32 sampleRate;
        if (const auto result = ma_decoder_get_data_format(decoder, &format, &channels, &sampleRate, nullptr, 0);
            result != MA_SUCCESS)
        {
            INSOUND_PUSH_ERROR(Result::MaErr, ma_result_description(result));
            delete decoder;
            return false;
        }

        if (m->decoder != nullptr) // clean up pre-existing decoder
        {
            ma_decoder_uninit(m->decoder);
            delete m->decoder;
        }

        m->decoder = decoder;
        m->looping = false;
        m->spec.channels = static_cast<int>(channels);
        m->spec.freq = static_cast<int>(sampleRate);
        m->spec.format = toFormat(format);
        m->targetSpec = targetSpec;
        return true;
    }

    void AudioDecoder::close()
    {
        if (m->decoder != nullptr)
        {
            ma_decoder_uninit(m->decoder);
            delete m->decoder;
            m->decoder = nullptr;
            m->pcmLength = UINT64_MAX;
        }
    }

    bool AudioDecoder::isOpen() const
    {
        return static_cast<bool>(m->decoder);
    }

    int AudioDecoder::readFrames(int sampleFrames, uint8_t *buffer)
    {
        INIT_GUARD();
        uint64_t pcmFrames;
        if (!getPCMFrameLength(&pcmFrames))
        {
            return 0;
        }

        uint64_t cursor;
        if (!getCursorPCMFrames(&cursor))
        {
            return 0;
        }

        ma_uint64 framesRead = 0;
        int framesLeft = static_cast<int>(pcmFrames - cursor);
        if (framesLeft <= 0)
        {
            if (!m->looping)
                return 0;
        }
        else
        {
            if (auto result = ma_decoder_read_pcm_frames(m->decoder, buffer, std::min(framesLeft, sampleFrames), &framesRead);
                result != MA_SUCCESS && result != MA_AT_END)
            {
                INSOUND_PUSH_ERROR(Result::MaErr, ma_result_description(result));
                return -1;
            }
        }

        if (m->looping)
        {
            while (true)
            {
                if (!getCursorPCMFrames(&cursor))
                {
                    return static_cast<int>(framesRead);
                }

                framesLeft = static_cast<int>(pcmFrames - cursor);
                if (framesLeft == 0)
                {
                    auto result = ma_decoder_seek_to_pcm_frame(m->decoder, 0);
                    if (result != MA_SUCCESS)
                    {
                        INSOUND_PUSH_ERROR(Result::MaErr, ma_result_description(result));
                        return static_cast<int>(framesRead);
                    }
                }

                if (framesRead >= sampleFrames)
                    break;
                framesRead += readFrames(
                    static_cast<int>(sampleFrames - framesRead),
                    buffer + framesRead * m->targetSpec.bytesPerFrame());
            }
        }
        return static_cast<int>(framesRead);
    }

    int AudioDecoder::readBytes(const int bytesToRead, uint8_t *buffer)
    {
        const auto k = (m->spec.format.bits() / CHAR_BIT) * m->spec.channels;
        const auto sampleFrames = bytesToRead / k;
        return readFrames(sampleFrames, buffer) * k;
    }

    bool AudioDecoder::getLooping(bool *outLooping) const
    {
        INIT_GUARD();
        if (outLooping)
            *outLooping = m->looping;
        return true;
    }

    bool AudioDecoder::setLooping(const bool value)
    {
        INIT_GUARD();
        m->looping = value;
        return true;
    }

    bool AudioDecoder::getPosition(TimeUnit units, double *outPosition) const
    {
        INIT_GUARD();
        ma_uint64 cursor;
        if (const auto result = ma_decoder_get_cursor_in_pcm_frames(m->decoder, &cursor);
            result != MA_SUCCESS)
        {
            INSOUND_PUSH_ERROR(Result::MaErr, ma_result_description(result));
            return false;
        }

        if (outPosition)
            *outPosition = convert(cursor, TimeUnit::PCM, units, m->targetSpec);
        return true;
    }

    bool AudioDecoder::setPosition(TimeUnit units, uint64_t position)
    {
        INIT_GUARD();
        const auto frame = std::round(convert(position, units, TimeUnit::PCM, m->targetSpec));
        if (const auto result = ma_decoder_seek_to_pcm_frame(m->decoder, static_cast<ma_uint64>(frame));
            result != MA_SUCCESS)
        {
            INSOUND_PUSH_ERROR(Result::MaErr, ma_result_description(result));
            return false;
        }

        return true;
    }

    bool AudioDecoder::getSpec(AudioSpec *outSpec) const
    {
        INIT_GUARD();
        if (outSpec)
            *outSpec = m->spec;
        return true;
    }

    bool AudioDecoder::getTargetSpec(AudioSpec *outTargetSpec) const
    {
        INIT_GUARD();
        if (outTargetSpec)
            *outTargetSpec = m->targetSpec;
        return true;
    }

    bool AudioDecoder::isEnded(bool *outEnded) const
    {
        INIT_GUARD();
        uint64_t available;
        if (!getAvailableFrames(&available))
            return false;

        if (outEnded)
            *outEnded = available == 0 || m->stream.isEof();
        return true;
    }

    bool AudioDecoder::getPCMFrameLength(uint64_t *outMaxFrames) const
    {
        INIT_GUARD();
        if (m->pcmLength == UINT64_MAX)
        {
            ma_uint64 frames;
            if (const auto result = ma_decoder_get_length_in_pcm_frames(m->decoder, &frames);
                result != MA_SUCCESS)
            {
                INSOUND_PUSH_ERROR(Result::MaErr, ma_result_description(result));
                return false;
            }

            m->pcmLength = frames;
        }

        if (outMaxFrames)
            *outMaxFrames = m->pcmLength;
        return true;
    }

    bool AudioDecoder::getCursorPCMFrames(uint64_t *outCursor) const
    {
        INIT_GUARD();
        ma_uint64 cursor;
        if (const auto result = ma_decoder_get_cursor_in_pcm_frames(m->decoder, &cursor);
            result != MA_SUCCESS)
        {
            INSOUND_PUSH_ERROR(Result::MaErr, ma_result_description(result));
            return false;
        }

        if (outCursor)
            *outCursor = cursor;

        return true;
    }

    bool AudioDecoder::getAvailableFrames(uint64_t *outFrames) const
    {
        INIT_GUARD();
        uint64_t pcmLength;
        if (!getPCMFrameLength(&pcmLength))
            return false;

        uint64_t cursor;
        if (!getCursorPCMFrames(&cursor))
            return false;

        if (cursor > pcmLength) // prevent negative overflow of unsigned type
            cursor = pcmLength;

        if (outFrames)
            *outFrames = pcmLength - cursor;
        return true;
    }
}
