#include "decodeVorbis.h"

#ifdef  INSOUND_DECODE_VORBIS
#include "../AudioSpec.h"
#include "../Error.h"

#include <stb_vorbis.c>

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
