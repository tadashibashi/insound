#pragma once
#include <cstdint>
#include <filesystem>

namespace insound {
    struct AudioSpec;

    /// Load audio from the filesystem and retrieve a buffer of PCM data converted to the desired format
    bool loadAudio(const std::filesystem::path &path, const AudioSpec &targetSpec,
        uint8_t **outBuffer, uint32_t *outLength);

    /// Convert audio from one format to another
    /// @param audioData       sample data pointer; function takes ownership, and it is no longer valid, retrieve resultant pointer in `outBuffer`
    /// @param length     length of the data buffer
    /// @param dataSpec   specification of the sample data passed in
    /// @param targetSpec target specification to convert to
    /// @param outBuffer  [out] pointer to receive the converted buffer
    /// @param outLength  [out] pointer to receive length of the `outBuffer`
    bool convertAudio(uint8_t *audioData, uint32_t length, const AudioSpec &dataSpec,
                      const AudioSpec &targetSpec, uint8_t **outBuffer, uint32_t *outLength);
}
