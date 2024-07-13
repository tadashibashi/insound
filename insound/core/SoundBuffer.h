#pragma once

#include "AudioSpec.h"

#include <atomic>

#include <string>

namespace insound {

    /// Container holding PCM sound data
    class SoundBuffer {
    public:
        SoundBuffer();
        SoundBuffer(const std::string &filepath, const AudioSpec &targetSpec);
        ~SoundBuffer();

        SoundBuffer(SoundBuffer &&other) noexcept;

        /// Load sound and convert to target specification
        /// @param filepath    path to the sound file (only WAV supported for now)
        /// @param targetSpec  the specification to convert this buffer to on load, usually of the opened audio device
        ///                    to match the output type
        bool load(const std::string &filepath, const AudioSpec &targetSpec);

        /// Free sound buffer resources
        void unload();

        /// Check if sound is currently loaded with data
        [[nodiscard]]
        bool isLoaded() const { return m_buffer.load() != nullptr; }

        /// Size of buffer in bytes
        [[nodiscard]]
        auto size() const { return m_bufferSize; }

        /// Get the data buffer. If not loaded, `nullptr` will be returned.
        [[nodiscard]]
        uint8_t *data() { return m_buffer.load(); }

        /// Get the data buffer. If not loaded, `nullptr` will be returned.
        [[nodiscard]]
        const uint8_t *data() const { return m_buffer.load(); }

        [[nodiscard]]
        const AudioSpec &spec() const { return m_spec; }

        // Replace current buffer with a new one
        void emplace(uint8_t *buffer, uint32_t bufferSize, const AudioSpec &spec);
    private:
        uint32_t  m_bufferSize;
        std::atomic<uint8_t *> m_buffer;
        AudioSpec m_spec;
    };

}

