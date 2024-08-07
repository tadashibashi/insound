#pragma once
#include "../Marker.h"

#include <cstdint>
#include <vector>

namespace insound {
    struct AudioSpec;

    /// Load audio from the filesystem and retrieve a buffer of PCM data converted to the desired format.
    /// For now, only the WAV format parses marker information.
    ///
    /// @param path        path to the file to load. On html5 platform, you may load from an http URL if this function is run
    ///                    in another thread. AudioLoader implements this, as an example.
    /// @param targetSpec  specification to convert this audio format to.
    bool loadAudio(const std::string &path, const AudioSpec &targetSpec,
        uint8_t **outBuffer, uint32_t *outLength, std::vector<Marker> *outMarkers);

    /// Convert audio from one format to another. Intended for single use.
    /// @param audioData  sample data pointer; function takes ownership, and it is no longer valid, retrieve resultant pointer in `outBuffer`
    /// @param length     length of the data buffer
    /// @param dataSpec   specification of the sample data passed in
    /// @param targetSpec target specification to convert to
    /// @param outBuffer  [out] pointer to receive the converted buffer, safe to use the pointer passed into `audioData`
    /// @param outLength  [out] pointer to receive length of the `outBuffer`
    bool convertAudio(uint8_t *audioData, uint32_t length, const AudioSpec &dataSpec,
                      const AudioSpec &targetSpec, uint8_t **outBuffer, uint32_t *outLength);
}
