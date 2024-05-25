#pragma once

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

struct SDL_AudioSpec;

namespace insound {
    /// Container holding PCM sound data
    class SoundBuffer {
    public:
        SoundBuffer();
        ~SoundBuffer();

        /// Load sound and convert to target specification
        /// @param filepath    path to the sound file (only WAV supported for now)
        /// @param targetSpec  the specification to convert this buffer to on load, usually of the opened audio device
        ///                    to match the output type
        bool load(const fs::path &filepath, const SDL_AudioSpec &targetSpec);

        /// Free sound buffer resources
        void unload();

        /// Check if sound is currently loaded with data
        [[nodiscard]]
        bool isLoaded() const { return m_buffer != nullptr; }

        /// Size of buffer in bytes
        [[nodiscard]]
        auto size() const { return m_byteLength; }



        /// Get the data buffer. If not loaded, `nullptr` will be returned.
        [[nodiscard]]
        uint8_t *data() { return m_buffer; }

        /// Get the data buffer. If not loaded, `nullptr` will be returned.
        [[nodiscard]]
        const uint8_t *data() const { return m_buffer; }

        [[nodiscard]]
        bool isInt() const;

        [[nodiscard]]
        bool isFloat() const;

        [[nodiscard]]
        bool isSigned() const;

        [[nodiscard]]
        bool isUnsigned() const;

        [[nodiscard]]
        int samplerate() const;

        [[nodiscard]]
        int bitSize() const;

        [[nodiscard]]
        uint8_t channels() const;
    private:
        // buffer
        uint32_t m_byteLength;
        uint8_t *m_buffer;

        // buffer specs
        int m_freq;
        uint8_t m_channels;
        uint16_t m_format;

    };

}

