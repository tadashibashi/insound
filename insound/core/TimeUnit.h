#pragma once
#include "AudioSpec.h"

#include <cstdint>

namespace insound {

    /// Units to measure time.
    /// Insound stores its time units in PCM samples, where samplerate is the source of truth.
    enum class TimeUnit {
        Micros,    ///< Microseconds: milliseconds / 1000
        Millis,    ///< Milliseconds: seconds / 1000
        PCM,       ///< PCM samples:  samplerate * seconds
        PCMBytes,  ///< PCM bytes:    frames * channels * data width (32-bit => 4 bytes)
    };

    /// Convert from one time unit to another
    /// @param value  number of `source` units
    /// @param source source units to convert from
    /// @param target target units to convert to
    /// @param spec   current engine audio spec
    /// @returns double precision float, allowing user to round or truncate as needed.
    ///          If number is less than 0 an error occured. Check popError() for details.
    double convert(uint64_t value, TimeUnit source, TimeUnit target, const AudioSpec &spec);
}
