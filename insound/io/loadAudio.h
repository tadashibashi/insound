#pragma once
#include <cstdint>
#include <filesystem>

namespace insound {
    struct AudioSpec;

    bool loadAudio(const std::filesystem::path &path, const AudioSpec &targetSpec, uint8_t **outBuffer, uint32_t *outLength);

    /// Convert audio from one format to another
    /// @param data       sample data pointer; function takes ownership, and it is no longer valid, retrieve resultant pointer in `outBuffer`
    /// @param dataSpec   specification of the sample data passed in
    /// @param targetSpec target specification to convert to
    /// @param length     length of the data buffer
    /// @param outLength  [out] pointer to receive length of the `outBuffer`
    /// @param outBuffer  [out] pointer to receive the converted buffer
    bool convertAudio(uint8_t *data, const AudioSpec &dataSpec, const AudioSpec &targetSpec,
                      uint32_t length, uint32_t *outLength, uint8_t **outBuffer);
}
