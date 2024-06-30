#pragma once
#if INSOUND_DECODE_MP3
#include "../AudioDecoder.h"
#include "../AudioSpec.h"

#include <cstdint>
#include <filesystem>

namespace insound {
    /// Decode an entire MP3 file into PCM data
    /// @param memory        pointer to mp3 file data
    /// @param size          size of memory buffer
    /// @param outSpec       [out] specification of the pcm data to receive
    /// @param outData       [out] pcm data buffer to receive
    /// @param outBufferSize [out] size of the pcm data buffer to receive
    bool decodeMp3(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outData,
               uint32_t *outBufferSize);


    /// Stream an mp3 pcm data from a file. Does not perform any conversion.
    class Mp3Decoder final : public AudioDecoder {
    public:
        Mp3Decoder();
        Mp3Decoder(Mp3Decoder &&other) noexcept;
        ~Mp3Decoder() override;
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
#endif
