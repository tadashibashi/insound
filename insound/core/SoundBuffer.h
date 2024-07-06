#pragma once

#include "AudioSpec.h"
#include "Marker.h"
#include "TimeUnit.h"

#include <atomic>

#include <string>
#include <vector>

namespace insound {

    /// Container holding PCM sound data
    class SoundBuffer {
    public:
        SoundBuffer();
        SoundBuffer(const std::string &filepath, const AudioSpec &targetSpec);
        SoundBuffer(uint8_t *m_buffer, uint32_t bufferSize, const AudioSpec &spec, std::vector<Marker> &&markers);
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

        /// Convert audio buffer to another format. Blocking.
        /// Danger: do not call this while buffer's pcm audio is playing.
        /// This function is mainly for ease of conversion if we ever support handling audio rate / format changes.
        /// @param newSpec format specification target
        /// @returns whether conversion succeeded
        bool convert(const AudioSpec &newSpec);

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

        void addMarker(const std::string &label, TimeUnit unit, uint64_t position);
        const Marker &getMarker(size_t index) const { return m_markers[index]; }
        size_t markerCount() const { return m_markers.size(); }
    private:
        uint32_t  m_bufferSize;
        std::atomic<uint8_t *> m_buffer;
        AudioSpec m_spec;
        std::vector<Marker> m_markers;
    };

}

