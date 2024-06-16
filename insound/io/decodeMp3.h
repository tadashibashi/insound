#pragma once
#include <insound/AudioSpec.h>

#include <cstdint>

namespace insound {
    /// Decode an MP3 file into PCM data
    /// @param memory        pointer to mp3 file data
    /// @param size          size of memory buffer
    /// @param outSpec       [out] specification of the pcm data to receive
    /// @param outData       [out] pcm data buffer to receive
    /// @param outBufferSize [out] size of the pcm data buffer to receive
    bool decodeMp3(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outData,
               uint32_t *outBufferSize);
}
