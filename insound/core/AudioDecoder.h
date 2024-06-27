#pragma once
#include "AudioSpec.h"
#include "TimeUnit.h"

#include <filesystem>

namespace insound {
    namespace fs = std::filesystem;

    /// Abstract base class for audio decoders for streaming PCM data from files
    class AudioDecoder {
    public:

        virtual ~AudioDecoder() = default;

        /// Open the file for streaming
        virtual bool open(const fs::path &filepath) = 0;
        /// Close an open file. Safe to call, even if already closed.
        virtual void close() = 0;
        [[nodiscard]]
        virtual bool isOpen() const = 0;

        /// Read next samples into a data buffer.
        /// @param sampleFrames number of frames to read
        /// @param buffer       buffer to write PCM data to
        /// @returns number of frames read
        virtual int read(int sampleFrames, uint8_t *buffer) = 0;
        int readBytes(int bytesToRead, uint8_t *buffer);

        bool getLooping(bool *outLooping) const;
        bool setLooping(bool value);

        /// Get the current seek position
        /// @param       units              time units to retrieve position in
        /// @param [out] outPosition position to retrieve in `units`
        /// @returns whether function was successful, check `popError()` for more info on `false`.
        virtual bool getPosition(TimeUnit units, double *outPosition) const = 0;

        /// Seek to a position in the track
        /// @param units       time units to set
        /// @param position position to seek to in `units`
        virtual bool setPosition(TimeUnit units, uint64_t position) = 0;

        bool getSpec(AudioSpec *outSpec) const;

    protected:
        AudioDecoder() : m_spec(), m_looping(false) { }
        AudioDecoder(AudioDecoder &&other) noexcept : m_spec(other.m_spec), m_looping(other.m_looping) { }
        AudioSpec m_spec;
        bool m_looping;
    };
}
