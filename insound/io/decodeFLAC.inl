#pragma once
#include "../AudioSpec.h"
#include "../Error.h"

#include <cstdint>
#include <filesystem>

#ifdef INSOUND_DECODE_FLAC
#include "openFile.h"

#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>

#include <string>

namespace insound {
    inline bool decodeFLAC(const std::filesystem::path &path, AudioSpec *outSpec, uint8_t **outData,
        uint32_t *outBufferSize)
    {
        std::string fileData;
        if (!openFile(path, &fileData))
        {
            return false;
        }

        unsigned channels, samplerate;
        drflac_uint64 frameCount;
        auto pcmData = drflac_open_memory_and_read_pcm_frames_f32(
            fileData.data(), fileData.size(), &channels,
            &samplerate, &frameCount, nullptr);

        if (!pcmData)
        {
            pushError(Result::RuntimeErr, "FLAC file failed to decode");
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
            *outBufferSize = frameCount * channels * sizeof(float);
        }

        return true;
    }
}
#else
namespace insound {
    inline bool decodeFLAC(const std::filesystem::path &path, AudioSpec *outSpec, uint8_t **outData,
        uint32_t *outBufferSize)
    {
        pushError(Result::NotSupported, "FLAC decoding is not supported, make sure to compile with "
            "INSOUND_DECODE_FLAC defined");
        return false;
    }
}
#endif