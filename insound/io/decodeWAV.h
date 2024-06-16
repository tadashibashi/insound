#pragma once

#include <insound/AudioSpec.h>

#include <cstdint>
#include <vector>

namespace insound {
    struct Cue {
        std::string name;     ///< string
        uint32_t    position; ///< position in samples, check sample rate retrieved from spec for conversion to seconds, etc.
    };
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

    WAVResult::Enum decodeWAV_v2(const uint8_t *memory, const uint32_t size,
                                 AudioSpec *outSpec, uint8_t **outBuffer, uint32_t *outBufferSize, std::vector<Cue> *outCues);
}
