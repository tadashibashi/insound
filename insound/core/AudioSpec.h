#pragma once
#include "SampleFormat.h"

namespace insound {
    /// Description for data in a buffer or stream of audio
    struct AudioSpec {
        AudioSpec() : freq(), channels(), format() { }
        AudioSpec(int freq, int channels, SampleFormat format) :
            freq(freq), channels(channels), format(format) { }

        int freq;             ///< number of sample frames per second
        int channels;         ///< number of interleaved channels per frame
        SampleFormat format;  ///< sample type info  (matches SDL2's audio format flags)

        [[nodiscard]]
        uint32_t bytesPerChannel() const { return format.bytes(); }
        [[nodiscard]]
        uint32_t bytesPerFrame() const { return format.bytes() * channels; }
        [[nodiscard]]
        uint32_t bytesPerSecond() const { return format.bytes() * channels * freq; }
    };
}
