#pragma once
#include "../AudioDecoder.h"
#include "../AudioSpec.h"

#include <cstdint>

namespace insound {
    bool decodeGME(const uint8_t *memory, uint32_t size, int samplerate, int track, int lengthMS, AudioSpec *outSpec, uint8_t **outData,
                   uint32_t *outBufferSize);

    /// Stream a GME file
    class GmeDecoder : public AudioDecoder {
    public:
        explicit GmeDecoder(int samplerate = 44100);
        GmeDecoder(GmeDecoder &&other) noexcept;
        ~GmeDecoder() override;
        bool open(const fs::path &path) override;
        void close() override;

        bool setPosition(TimeUnit units, uint64_t position) override;
        bool getPosition(TimeUnit units, double *outPosition) const override;

        bool setTrack(int track);
        bool getTrack(int *outTrack) const;
        bool getTrackCount(int *outTrackCount) const;

        [[nodiscard]]
        bool isOpen() const override;
        int read(int sampleFrames, uint8_t *data) override;

        bool isEnded(bool *outEnded) const override;

    private:
        struct Impl;
        Impl *m;
    };
}
