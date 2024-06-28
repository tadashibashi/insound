#include "decodeWAV.h"

#include "../AudioSpec.h"
#include "../BufferView.h"
#include "../Error.h"

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>
#include <insound/core/logging.h>

#include "Rstream.h"
#include "Rstreamable.h"

namespace insound {
    bool decodeWAV(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outBuffer,
        uint32_t *outBufferSize, std::map<uint32_t, Marker> *outCues)
    {
        drwav wav;
        if (!drwav_init_memory_with_metadata(&wav, memory, size, DRWAV_SEQUENTIAL, nullptr))
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "drwav_init_memory_with_metadata failed");
            return false;
        }

        if (outSpec)
        {
            AudioSpec spec;
            spec.channels = wav.fmt.channels;
            spec.freq = static_cast<int>(wav.fmt.sampleRate);
            spec.format = SampleFormat(32, true, false, true);
            *outSpec = spec;
        }

        const auto bufferSize = sizeof(float) * wav.totalPCMFrameCount * wav.channels;
        if (outBuffer)
        {
            const auto samples = static_cast<float *>(std::malloc(bufferSize));
            auto framesRead = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, samples);
            if (framesRead < wav.totalPCMFrameCount)
            {
                INSOUND_PUSH_ERROR(Result::RuntimeErr, "drwav_read_pcm_frames_f32 failed");
                std::free(samples);
                return false;
            }

            *outBuffer = reinterpret_cast<uint8_t *>(samples);
        }

        if (outCues && wav.pMetadata)
        {
            std::map<uint32_t, Marker> cues;

            for (uint32_t i = 0; i < wav.metadataCount; ++i)
            {
                const auto &[type, data] = wav.pMetadata[i];
                switch(type)
                {
                    case drwav_metadata_type_cue:
                    {
                        for (uint32_t cueIndex = 0; cueIndex < data.cue.cuePointCount; ++cueIndex)
                        {
                            const auto &cue = data.cue.pCuePoints[cueIndex];
                            cues[cue.id].position = cue.sampleByteOffset;
                        }
                    } break;

                    case drwav_metadata_type_list_label:
                    {
                        cues[data.labelOrNote.cuePointId].label =
                            std::string(data.labelOrNote.pString, data.labelOrNote.stringLength);
                    } break;

                    default:
                        break;
                }
            }

            outCues->swap(cues);
        }

        if (outBufferSize)
        {
            *outBufferSize = static_cast<uint32_t>(bufferSize);
        }

        return true;
    }

    static size_t drwav_on_read_rstream(void *userData, void *bufferOut, const size_t bytesToRead)
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

    static drwav_bool32 drwav_on_seek_rstream(void *userData, const int offset, const drwav_seek_origin origin)
    {
        const auto stream = static_cast<Rstreamable *>(userData);
        const int64_t seekPosition = (origin == drwav_seek_origin_current) ?
            offset + stream->position() : offset;
        return stream->seek(seekPosition);
    }


#if INSOUND_DEBUG
#define INIT_GUARD() do { if (!isOpen()) { \
    INSOUND_PUSH_ERROR(Result::DecoderNotInit, __FUNCTION__); \
    return false; \
} } while(0)
#else
#define INIT_GUARD()
#endif
    struct WavDecoder::Impl {
        Impl() : wav{}, stream{}, cursor{} { }
        drwav wav;
        Rstream stream;
        uint32_t cursor;
        uint64_t lengthPCM;
    };

    WavDecoder::WavDecoder() : m(new Impl)
    { }

    WavDecoder::WavDecoder(WavDecoder &&other) noexcept : m(other.m)
    {
        other.m = nullptr;
    }

    WavDecoder::~WavDecoder()
    {
        delete m;
    }

    bool WavDecoder::open(const fs::path &path)
    {
        if (!m->stream.open(path))
        {
            return false;
        }

        drwav wav;
        if (!drwav_init(&wav, drwav_on_read_rstream, drwav_on_seek_rstream,
            m->stream.stream(), nullptr))
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "Wav file failed to open/init");
            return false;
        }

        drwav_uint64 lengthPCM;
        if (drwav_get_length_in_pcm_frames(&wav, &lengthPCM) != DRWAV_SUCCESS)
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "drwav failed to get wav pcm length");
            return false;
        }

        if (isOpen())
        {
            drwav_uninit(&m->wav);
        }

        m->wav = wav;
        m_spec.freq = static_cast<int>(wav.sampleRate);
        m_spec.channels = static_cast<int>(wav.channels);
        m_spec.format = SampleFormat(sizeof(float) * CHAR_BIT,
            true,
            false,
            true);
        m->lengthPCM = lengthPCM;
        return true;
    }

    bool WavDecoder::isOpen() const
    {
        return m && m->wav.channels > 0;
    }

    void WavDecoder::close()
    {
        if (isOpen())
        {
            drwav_uninit(&m->wav);
            std::memset(&m->wav, 0, sizeof(drwav));

            m->stream.close();
        }
    }

    bool WavDecoder::setPosition(TimeUnit units, uint64_t position)
    {
        INIT_GUARD();

        const auto targetPCMFrame = std::round(convert(position, units, TimeUnit::PCM, m_spec));
        const auto result = drwav_seek_to_pcm_frame(&m->wav, static_cast<uint64_t>(targetPCMFrame));

        return static_cast<bool>(result);
    }

    bool WavDecoder::getPosition(const TimeUnit units, double *outPosition) const
    {
        INIT_GUARD();

        drwav_uint64 cursor;
        if (!drwav_get_cursor_in_pcm_frames(&m->wav, &cursor))
        {
            return false;
        }

        if (outPosition)
        {
            *outPosition = convert(cursor, TimeUnit::PCM, units, m_spec);
        }
        return true;
    }

    int WavDecoder::read(int sampleFrames, uint8_t *data)
    {
        INIT_GUARD();

        if (!m_looping && m->wav.readCursorInPCMFrames + sampleFrames > m->lengthPCM)
        {
            // Read partial block
            const auto framesToRead = m->lengthPCM - m->wav.readCursorInPCMFrames;
            if (framesToRead == 0)
                return 0;

            return static_cast<int>(
                drwav_read_pcm_frames_f32(
                    &m->wav, framesToRead, reinterpret_cast<float *>(data)));
        }

        // Read whole block
        auto framesRead = static_cast<int>(
            drwav_read_pcm_frames_f32(&m->wav, sampleFrames, reinterpret_cast<float *>(data)));

        if (m_looping)
        {
            while(true)
            {
                INSOUND_LOG("current: %llu, length: %llu\n", m->wav.readCursorInPCMFrames, m->lengthPCM);
                if (m->wav.readCursorInPCMFrames >= m->lengthPCM)
                {
                    if (!drwav_seek_to_first_pcm_frame(&m->wav))
                    {
                        INSOUND_PUSH_ERROR(Result::RuntimeErr, "drwav failed to seek to first PCM frame");
                        return framesRead;
                    }
                }

                if (framesRead >= sampleFrames)
                    break;

                framesRead += read(sampleFrames - framesRead, (uint8_t *)((float *)data + framesRead/2));
            }
        }

        return framesRead;
    }

    bool WavDecoder::isEnded(bool *outEnded) const
    {
        INIT_GUARD();
        if (outEnded)
            *outEnded = m->wav.readCursorInPCMFrames >= m->lengthPCM;
        return true;
    }
}
