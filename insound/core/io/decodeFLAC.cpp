#include "decodeFLAC.h"

#include "../Error.h"

#ifdef INSOUND_DECODE_FLAC

#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>

namespace insound {
    bool decodeFLAC(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outData,
        uint32_t *outBufferSize)
    {
        unsigned channels, samplerate;
        drflac_uint64 frameCount;
        auto pcmData = drflac_open_memory_and_read_pcm_frames_f32(
            memory, size, &channels,
            &samplerate, &frameCount, nullptr);

        if (!pcmData)
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "FLAC file failed to decode");
            return false;
        }

        if (outSpec)
        {
            AudioSpec spec;
            spec.channels = (int)channels;
            spec.freq = (int)samplerate;
            spec.format = SampleFormat(32, true, false, true);

            *outSpec = spec;
        }

        if (outData)
        {
            *outData = (uint8_t *)pcmData;
        }
        else
        {
            drflac_free(pcmData, nullptr);
        }

        if (outBufferSize)
        {
            *outBufferSize = (uint32_t)frameCount * (uint32_t)channels * (uint32_t)sizeof(float);
        }

        return true;
    }
}
#else
namespace insound {
    bool decodeFLAC(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outData,
        uint32_t *outBufferSize)
    {
        INSOUND_PUSH_ERROR(Result::NotSupported, "FLAC decoding is not supported, make sure to compile with "
            "INSOUND_DECODE_FLAC defined");
        return false;
    }
}
#endif
