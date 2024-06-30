#include "decodeVorbis.h"

#ifdef  INSOUND_DECODE_VORBIS
#include "../AudioSpec.h"
#include "../Error.h"
#include "Rstream.h"
#include "Rstreamable.h"

#include "../external/stb_vorbis.h"
#include <insound/core/AlignedVector.h>
#include <insound/core/BufferView.h>
namespace insound {

    bool decodeVorbis(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outData,
        uint32_t *outBufferSize)
    {
        int channels, samplerate;
        short *data;
        const auto sampleCount = stb_vorbis_decode_memory(
            (const unsigned char *)memory,
            (int)size,
            &channels,
            &samplerate,
            &data);

        // auto vorbis = stb_vorbis_open_memory(memory, size, nullptr, nullptr);
        // if (!vorbis)
        // {
        //     INSOUND_PUSH_ERROR(Result::RuntimeErr, "decodeVorbis: failed to open from memory");
        //     return false;
        // }
        //
        // auto info = stb_vorbis_get_info(vorbis);
        // channels = info.channels;
        // samplerate = info.sample_rate;
        //
        // stb_vorbis_get_samples_float_interleaved(vorbis, 2, &)


        if (sampleCount < 0)
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "decodeVorbis: failed to decode vorbis file");
            return false;
        }

        if (outSpec)
        {
            AudioSpec spec;
            spec.channels = channels;
            spec.freq = samplerate;
            spec.format = SampleFormat(sizeof(short) * CHAR_BIT, false, false, true);

            *outSpec = spec;
        }

        if (outData)
        {
            *outData = (uint8_t *)data;
        }
        else
        {
            free(outData);
        }

        if (outBufferSize)
        {
            *outBufferSize = sampleCount * channels * (uint32_t)sizeof(short);
        }

        return true;
    }

    struct VorbisDecoder::Impl {
        stb_vorbis *vorbis{};
        AlignedVector<uint8_t, 16> buffer{};
    };

    VorbisDecoder::VorbisDecoder() : m(new Impl)
    {
    }

    VorbisDecoder::VorbisDecoder(VorbisDecoder &&other) noexcept : m(other.m)
    {
        other.m = nullptr;
    }

    VorbisDecoder::~VorbisDecoder()
    {
        delete m;
    }

    bool VorbisDecoder::open(const fs::path &path)
    {
        int error = 0;
        auto vorbis = stb_vorbis_open_filename(path.c_str(), &error, nullptr);
        if (!vorbis)
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "Vorbis failed to open");
            return false;
        }
        const auto info = stb_vorbis_get_info(vorbis);

        m_spec.channels = 2;
        m_spec.freq = static_cast<int>(info.sample_rate);
        m_spec.format = SampleFormat(32, true, false, true);

        if (m->vorbis)
        {
            stb_vorbis_close(m->vorbis);
        }

        m->vorbis = vorbis;
        return true;
    }

    void VorbisDecoder::close()
    {
        if (m->vorbis)
        {
            stb_vorbis_close(m->vorbis);
            m->vorbis = nullptr;
            m->buffer.clear();
        }
    }

    bool VorbisDecoder::setPosition(const TimeUnit units, const uint64_t position)
    {
        const auto sampleFrame = static_cast<unsigned>(std::round(convert(position, units, TimeUnit::PCM, m_spec)));
        return stb_vorbis_seek(m->vorbis, sampleFrame); // TODO: check if sample frames or samples
    }

    bool VorbisDecoder::getPosition(TimeUnit units, double *outPosition) const
    {
        if (outPosition)
        {
            const auto sampleFrame = stb_vorbis_get_sample_offset(m->vorbis) / m->vorbis->channels;
            *outPosition = convert(sampleFrame, TimeUnit::PCM, units, m_spec);
        }

        return true;
    }

    bool VorbisDecoder::isOpen() const
    {
        return m && m->vorbis && m->vorbis->f && m->vorbis->f->isOpen();
    }

    int VorbisDecoder::read(int sampleFrames, uint8_t *data)
    {
        const auto targetSamples = sampleFrames * 2;
        int samplesRead = 0;
        while (samplesRead < targetSamples)
        {
            const auto curSamplesRead = stb_vorbis_get_samples_float_interleaved(
                m->vorbis, 2, reinterpret_cast<float *>(data) + samplesRead, targetSamples - samplesRead);
            if (curSamplesRead <= 0) // eof
                break;

            samplesRead += curSamplesRead * 2;
        }

        return samplesRead / 2;
    }

    bool VorbisDecoder::isEnded(bool *outEnded) const
    {
        *outEnded = m->vorbis->eof;
        return true;
    }

    bool VorbisDecoder::getMaxFrames(uint64_t *outMaxFrames) const
    {
        if (!m->vorbis)
            return false;
        if (outMaxFrames)
            *outMaxFrames = stb_vorbis_stream_length_in_samples(m->vorbis) / m->vorbis->channels;
        return true;
    }
}
#else
namespace insound {
    bool decodeVorbis(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outData,
        uint32_t *outBufferSize)
    {
        INSOUND_PUSH_ERROR(Result::NotSupported, "Vorbis decoding is not supported, make sure to compile with "
            "INSOUND_DECODE_VORBIS defined");
        return false;
    }
}
#endif
