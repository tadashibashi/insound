#pragma once

#include "AudioSpec.h"

namespace insound {
    /// Handles conversions of channel and sample formats
    class DataConverter {
    public:
        DataConverter();
        DataConverter(const AudioSpec &input, const AudioSpec &output);
        ~DataConverter();

        void setSpecs(const AudioSpec &input, const AudioSpec &output);
        [[nodiscard]] const AudioSpec &inputSpec() const;
        [[nodiscard]] const AudioSpec &outputSpec() const;

        [[nodiscard]] uint64_t toInFrames(uint64_t outputFrames) const;
        [[nodiscard]] uint64_t toOutFrames(uint64_t inputFrames) const;

        bool process(uint8_t *inFrames, uint64_t inFrameCount, uint8_t *outFrames, uint64_t *outFrameCount);

        /// Intended for a single conversion of an entire buffer at one time.
        /// Buffer is resized, if necessary and returned. If this function returns true, the initial pointer will be
        /// invalidated and a new one is returned, filled with the sample data.
        bool convert(uint8_t **inFrames, uint64_t inFrameCount);
    private:
        struct Impl;
        Impl *m;
    };
}
