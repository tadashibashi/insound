#include "decodeMp3.h"

#include <insound/Error.h>

#ifdef INSOUND_DECODE_MP3

#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>

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
            pushError(Result::RuntimeErr, "MP3 file failed to decode");
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
}
#else
namespace insound {
    bool decodeMp3(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outData,
        uint32_t *outBufferSize)
    {
        pushError(Result::NotSupported, "MP3 decoding is not supported, make sure to compile with "
            "INSOUND_DECODE_MP3 defined");
        return false;
    }
}
#endif
