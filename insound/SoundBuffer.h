#pragma once

#include <filesystem>
#include <vector>

#include "AudioSpec.h"
#include "SampleFormat.h"

namespace fs = std::filesystem;

struct SDL_AudioSpec;

namespace insound {
    /// Container holding PCM sound data
    class SoundBuffer {
    public:
        SoundBuffer();
        SoundBuffer(const fs::path &filepath, const AudioSpec &targetSpec);
        ~SoundBuffer();

        /// Load sound and convert to target specification
        /// @param filepath    path to the sound file (only WAV supported for now)
        /// @param targetSpec  the specification to convert this buffer to on load, usually of the opened audio device
        ///                    to match the output type
        bool load(const fs::path &filepath, const AudioSpec &targetSpec);

        /// Free sound buffer resources
        void unload();

        /// Check if sound is currently loaded with data
        [[nodiscard]]
        bool isLoaded() const { return m_buffer != nullptr; }

        /// Size of buffer in bytes
        [[nodiscard]]
        auto size() const { return m_bufferSize; }



        /// Get the data buffer. If not loaded, `nullptr` will be returned.
        [[nodiscard]]
        uint8_t *data() { return m_buffer; }

        /// Get the data buffer. If not loaded, `nullptr` will be returned.
        [[nodiscard]]
        const uint8_t *data() const { return m_buffer; }

        [[nodiscard]]
        const AudioSpec &spec() const { return m_spec; }
    private:
        uint32_t m_bufferSize;
        uint8_t *m_buffer;
        AudioSpec m_spec;
    };

}

