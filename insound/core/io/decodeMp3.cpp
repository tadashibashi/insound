#include "decodeMp3.h"

#include "../Error.h"

#ifdef INSOUND_DECODE_MP3

#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>
#include "Rstream.h"
#include "Rstreamable.h"

namespace insound {

    bool decodeMp3(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outData,
        uint32_t *outBufferSize)
    {
        drmp3_uint64 frameCount;
        drmp3_config config;
        auto pcmData = drmp3_open_memory_and_read_pcm_frames_f32(memory, size, &config,
            &frameCount, nullptr);

        if (!pcmData)
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "MP3 file failed to decode");
            return false;
        }

        if (outSpec)
        {
            AudioSpec spec;
            spec.channels = (int)config.channels;
            spec.freq = (int)config.sampleRate;
            spec.format = SampleFormat(32, true, false, true);

            *outSpec = spec;
        }

        if (outData)
        {
            *outData = (uint8_t *)pcmData;
        }
        else
        {
            drmp3_free(pcmData, nullptr);
        }

        if (outBufferSize)
        {
            *outBufferSize = (uint32_t)frameCount * (uint32_t)config.channels * (uint32_t)sizeof(float);
        }

        return true;
    }

#if INSOUND_DEBUG
#define INIT_GUARD() do { if (!isOpen()) { \
        INSOUND_PUSH_ERROR(Result::DecoderNotInit, __FUNCTION__); \
        return false; \
    } } while(0)
#else
#define INIT_GUARD()
#endif
    static size_t drmp3_on_read_rstream(void *userData, void *bufferOut, const size_t bytesToRead)
    {
        const auto stream = static_cast<Rstreamable *>(userData);
        const auto bytesRead = stream->read(static_cast<uint8_t *>(bufferOut), static_cast<int64_t>(bytesToRead));
        if (bytesRead < 0)
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "Failed to read bytes from Rstreamable");
            return 0;
        }

        return bytesRead;
    }

    static drmp3_bool32 drmp3_on_seek_rstream(void *userData, const int offset, const drmp3_seek_origin origin)
    {
        const auto stream = static_cast<Rstreamable *>(userData);
        const auto seekPosition = (origin == drmp3_seek_origin_current) ?
                offset + stream->position() : offset;
        return stream->seek(seekPosition);
    }

    struct Mp3Decoder::Impl {
        Impl() : file{}, stream{}
        {}

        drmp3 file;
        Rstream stream;
        uint64_t lengthPCM;
    };

    Mp3Decoder::Mp3Decoder() : m(new Impl)
    {
        // format will always read out as 32-bit float
        m_spec.format = SampleFormat(32, true, false, true);
    }

    Mp3Decoder::Mp3Decoder(Mp3Decoder &&other) noexcept : AudioDecoder(std::move(other)), m(other.m)
    {
        other.m = nullptr;
    }

    Mp3Decoder::~Mp3Decoder()
    {
        if (Mp3Decoder::isOpen())
        {
            drmp3_uninit(&m->file);
        }

        delete m;
    }

    bool Mp3Decoder::open(const fs::path &path)
    {
        if (!m->stream.open(path))
        {
            return false;
        }

        drmp3 file;
        if (!drmp3_init(&file, drmp3_on_read_rstream, drmp3_on_seek_rstream,
                        m->stream.stream(), nullptr))
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "Mp3 file failed to open/init");
            return false;
        }

        if (isOpen())
        {
            drmp3_uninit(&m->file);
        }

        m->file = file;
        m_spec.freq = file.sampleRate;
        m_spec.channels = file.channels;
        return true;
    }

    void Mp3Decoder::close()
    {
        if (isOpen())
        {
            drmp3_uninit(&m->file);
            std::memset(&m->file, 0, sizeof(drmp3));

            m->stream.close();
        }
    }

    bool Mp3Decoder::setPosition(const TimeUnit units, const uint64_t position)
    {
        INIT_GUARD();
        const auto targetPCMFrame = std::round(convert(position, units, TimeUnit::PCM, m_spec));
        const auto result = drmp3_seek_to_pcm_frame(&m->file, static_cast<uint64_t>(targetPCMFrame));

        return static_cast<bool>(result);
    }

    bool Mp3Decoder::getPosition(const TimeUnit units, double *outPosition) const
    {
        INIT_GUARD();
        const auto pcmFrame = m->file.currentPCMFrame;
        if (outPosition)
            *outPosition = convert(pcmFrame, TimeUnit::PCM, units, m_spec);
        return true;
    }

    bool Mp3Decoder::isOpen() const
    {
        return m && m->file.dataSize > 0;
    }

    int Mp3Decoder::read(const int sampleFrames, uint8_t *data)
    {
        INIT_GUARD();

        auto framesRead = static_cast<int>(drmp3_read_pcm_frames_f32(&m->file, sampleFrames, (float *)data));
        if (m_looping)
        {
            if (m->file.atEnd)
                drmp3_seek_to_start_of_stream(&m->file);

            if (framesRead < sampleFrames)
                framesRead += read(sampleFrames - framesRead, (uint8_t *)((float *)data + framesRead/2));
        }

        return framesRead;
    }
}

#endif
