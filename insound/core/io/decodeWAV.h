#pragma once

#include "../AudioSpec.h"
#include "../Marker.h"

#include <cstdint>
#include <map>

namespace insound {
    struct WAVResult {
        enum Enum {
            Ok,
            FileOpenErr,
            InvalidFile,
            RuntimeErr,
        };
    };

    /// More robust path since it covers non-PCM extensions - uses SDL to load WAV
    bool decodeWAV(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outBuffer, uint32_t *outBufferSize);

    WAVResult::Enum decodeWAV_v2(const uint8_t *memory, uint32_t size,
                                 AudioSpec *outSpec, uint8_t **outBuffer, uint32_t *outBufferSize, std::map<uint32_t, Marker> *outCues);
}
