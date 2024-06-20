#include "TimeUnit.h"
#include "Error.h"
#include <climits>

namespace insound {

    double convert(uint64_t value, TimeUnit source, TimeUnit target, const AudioSpec &spec)
    {
        if (source == target)
            return value;

        switch(source)
        {
            case TimeUnit::Micros: // microseconds to =========================>
            {
                switch(target)
                {
                    case TimeUnit::Millis:
                        return (double)(value / 1000ULL);

                    case TimeUnit::PCM:
                        return (double)(spec.freq * (value / 1000000.0));

                    case TimeUnit::PCMBytes:
                        return spec.channels * (spec.format.bits() / CHAR_BIT) * spec.freq * (value / 1000000.0);

                    default:
                        pushError(Result::InvalidArg, "Invalid target TimeUnit");
                        return -1.0;
                }
            } break;

            case TimeUnit::Millis: // milliseconds to ========================>
            {
                switch(target)
                {
                    case TimeUnit::Micros:
                        return (double)(value * 1000ULL);

                    case TimeUnit::PCM:
                        return (double)(spec.freq * (value / 1000.0));

                    case TimeUnit::PCMBytes:
                        return spec.channels * (spec.format.bits() / CHAR_BIT) * spec.freq * (value / 1000.0);

                    default:
                        pushError(Result::InvalidArg, "Invalid target TimeUnit");
                        return -1.0;

                }
            } break;

            case TimeUnit::PCM: // pcm sample frames to ======================>
            {
                switch(target)
                {
                    case TimeUnit::Micros:
                        return (double)value / (double)spec.freq * 1000000.0;

                    case TimeUnit::Millis:
                        return (double)value / (double)spec.freq * 1000.0;

                    case TimeUnit::PCMBytes:
                        return value * spec.channels * (spec.format.bits() / CHAR_BIT);

                    default:
                        pushError(Result::InvalidArg, "Invalid target TimeUnit");
                        return -1.0;
                }
            } break;

            case TimeUnit::PCMBytes: // pcm bytes to =========================>
            {
                switch(target)
                {
                    case TimeUnit::Micros:
                        return value / spec.channels / (spec.format.bits() / CHAR_BIT) * 1000000.0;

                    case TimeUnit::Millis:
                        return value / spec.channels / (spec.format.bits() / CHAR_BIT) * 1000.0;

                    case TimeUnit::PCM:
                        return value / spec.channels / (spec.format.bits() / CHAR_BIT);

                    default:
                        pushError(Result::InvalidArg, "Invalid target TimeUnit");
                        return -1.0;
                }
            } break;

            default:
                pushError(Result::InvalidArg, "Invalid source TimeUnit");
                return -1.0;
        }
    }
}
