#pragma once
#include <cstdint>
#include <string>
#include <utility>

namespace insound {
    struct Marker {
        Marker(): label(), position() { }
        Marker(std::string label, uint32_t samplePosition) : label(std::move(label)), position(samplePosition) { }

        std::string label;    ///< marker name
        uint32_t    position; ///< position in samples, check sample rate retrieved from spec for conversion to seconds, etc.
    };
}
