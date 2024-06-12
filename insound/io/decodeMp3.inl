#pragma once
#include "../AudioSpec.h"
#include "../Error.h"

#include <cstdint>
#include <filesystem>

#ifdef INSOUND_DECODE_MP3
#include "openFile.h"

#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>

#include <string>

namespace insound {
    inline bool decodeMp3(const std::filesystem::path &path, AudioSpec *outSpec, uint8_t **outData,
        uint32_t *outBufferSize)
    {
        std::string fileData;
        if (!openFile(path, &fileData))
        {
            return false;
        }

        drmp3_uint64 frameCount;
        drmp3_config config;
        auto pcmData = drmp3_open_memory_and_read_pcm_frames_f32(fileData.data(), fileData.size(), &config,
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
            drflac_free(pcmData, nullptr);
        }

        if (outBufferSize)
        {
            *outBufferSize = frameCount * config.channels * sizeof(float);
        }

        return true;
    }
}
#else
namespace insound {
    inline bool decodeMp3(const std::filesystem::path &path, AudioSpec *outSpec, uint8_t **outData,
        uint32_t *outBufferSize)
    {
        pushError(Result::NotSupported, "MP3 decoding is not supported, make sure to compile with "
            "INSOUND_DECODE_MP3 defined");
        return false;
    }
}
#endif