#pragma once
#include "../AudioDecoder.h"
#include "../AudioSpec.h"

#include <cstdint>
#include <filesystem>


namespace insound {
    /// Decode vorbis audio file into sample data
    /// @param memory        pointer to file memory
    /// @param size          length of memory in bytes
    /// @param outSpec       [out] pointer to receive audio spec data
    /// @param outData       [out] pointer to receive sample buffer
    /// @param outBufferSize [out] pointer to receive size of buffer in bytes
    /// @returns whether function succeeded, check `popError()` for details
    bool decodeVorbis(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outData,
                      uint32_t *outBufferSize);

    namespace fs = std::filesystem;

    /// Stream Vorbis data to PCM from a file. Does not perform any conversion.
    class VorbisDecoder final : public AudioDecoder {
    public:
        VorbisDecoder();
        VorbisDecoder(VorbisDecoder &&other) noexcept;
        ~VorbisDecoder() override;
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
