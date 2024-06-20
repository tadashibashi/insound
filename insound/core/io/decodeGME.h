#pragma once

#include "../AudioSpec.h"

#include <cstdint>

namespace insound {
    bool decodeGME(const uint8_t *memory, uint32_t size, int samplerate, int track, int lengthMS, AudioSpec *outSpec, uint8_t **outData,
                   uint32_t *outBufferSize);
}
