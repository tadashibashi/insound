#pragma once
#include "openFile.h"

#ifdef  INSOUND_DECODE_VORBIS
#include "../AudioSpec.h"
#include "../Error.h"

#include <stb_vorbis.c>
#include <fstream>

namespace insound {
    /// Decode vorbis audio file into sample data
    /// @param path          filepath to vorbis file
    /// @param outSpec       [out] pointer to receive audio spec data
    /// @param outData       [out] pointer to receive sample buffer
    /// @param outBufferSize [out] pointer to receive size of buffer in bytes
    /// @returns whether function succeeded, check `popError()` for details
    inline bool decodeVorbis(const std::filesystem::path &path, AudioSpec *outSpec, uint8_t **outData,
        uint32_t *outBufferSize)
    {
        std::string fileData;
        if (!openFile(path, &fileData))
        {
            return false;
        }

        int channels, samplerate;
        short *data;
        const auto sampleCount = stb_vorbis_decode_memory(
            (const unsigned char *)fileData.data(),
            (int)fileData.length(),
            &channels,
            &samplerate,
            &data);

        if (sampleCount < 0)
        {
            pushError(Result::RuntimeErr, "decodeVorbis: failed to decode vorbis file");
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
    inline bool decodeVorbis(const std::filesystem::path &path, AudioSpec *outSpec, uint8_t **outData,
        uint32_t *outBufferSize)
    {
        pushError(Result::NotSupported, "Vorbis decoding is not supported, make sure to compile with "
            "INSOUND_DECODE_VORBIS defined");
        return false;
    }
}
#endif