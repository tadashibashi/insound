#pragma once

#include <insound/AudioSpec.h>

#include <cstdint>

namespace insound {
    /// Decode a FLAC file into PCM data
    /// @param memory        pointer to FLAC file buffer
    /// @param size          size of memory in bytes
    /// @param outSpec       [out] specification of pcm buffer to receive
    /// @param outData       [out] pcm data buffer to receive
    /// @param outBufferSize [out] size of pcm data buffer in bytes to receive
    /// @returns whether function succeeded, check `popError()` for details
    bool decodeFLAC(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outData,
                    uint32_t *outBufferSize);
}
