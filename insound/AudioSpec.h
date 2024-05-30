#pragma once
#include "SampleFormat.h"

namespace insound {
    struct AudioSpec {
        AudioSpec(): freq(), channels(), format() { }

        int freq;             ///< number of frames per second
        int channels;         ///< number of interleaved channels per frame
        SampleFormat format;  ///< sample format info
    };
}
