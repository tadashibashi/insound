#pragma once

#include <insound/AudioSpec.h>

#include <cstdint>

namespace insound {
    /// Decode vorbis audio file into sample data
    /// @param memory        pointer to file memory
    /// @param size          length of memory in bytes
    /// @param outSpec       [out] pointer to receive audio spec data
    /// @param outData       [out] pointer to receive sample buffer
    /// @param outBufferSize [out] pointer to receive size of buffer in bytes
    /// @returns whether function succeeded, check `popError()` for details
    bool decodeVorbis(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outData,
                      uint32_t *outBufferSize);
}
