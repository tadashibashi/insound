#pragma once
#include "../AudioDecoder.h"
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
    bool decodeWAV(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outBuffer, uint32_t *outBufferSize, std::map<uint32_t, Marker> *outCues);

    /// Stream an mp3 pcm data from a file. Does not perform any conversion.
    class WavDecoder final : public AudioDecoder {
    public:
        WavDecoder();
        WavDecoder(WavDecoder &&other) noexcept;
        ~WavDecoder() override;
        bool open(const fs::path &path) override;
        void close() override;

        bool setPosition(TimeUnit units, uint64_t position) override;
        bool getPosition(TimeUnit units, double *outPosition) const override;

        [[nodiscard]]
        bool isOpen() const override;
        int read(int sampleFrames, uint8_t *data) override;

        bool isEnded(bool *outEnded) const override;
        bool getMaxFrames(uint64_t *outMaxFrames) const override;
    private:
        struct Impl;
        Impl *m;
    };
}
