#pragma once
#include "AudioSpec.h"
#include "TimeUnit.h"

#include <string>

namespace insound {

    /// Decoder for streaming PCM data from various audio files
    /// Supported formats: WAV, FLAC, VORBIS, ADPCM, AIFF, MP3, TODO: GME, ModPlug backends
    class AudioDecoder {
    public:
        /// Create an audio decoder by filepath name, and open file at path.

        /// @param filepath path to the audio file
        /// @param outputSpec spec of the audio engine output; needed by formats that generate their own output
        /// @returns pointer to the AudioDecoder subclass with opened file, or `nullptr` if any errors.
        ///          If null, check `popError()` for details on the error.
        static AudioDecoder *create(const std::string &filepath, const AudioSpec &outputSpec);
        AudioDecoder();
        AudioDecoder(AudioDecoder &&other) noexcept;
        ~AudioDecoder();

        AudioDecoder &operator=(AudioDecoder &&other) noexcept;

        /// Open the file for streaming
        bool open(const std::string &filepath, const AudioSpec &targetSpec, bool inMemory = false);
        /// Close an open file. Safe to call, even if already closed.
        void close();
        [[nodiscard]]
        bool isOpen() const;

        /// Read next samples into a data buffer.
        /// @param sampleFrames number of frames to read
        /// @param buffer       buffer to write PCM data to
        /// @returns number of frames read, 0 on error.
        int readFrames(int sampleFrames, uint8_t *buffer);
        int readBytes(int bytesToRead, uint8_t *buffer);

        bool getLooping(bool *outLooping) const;
        bool setLooping(bool value);

        /// Get the current seek position
        /// @param       units              time units to retrieve position in
        /// @param [out] outPosition position to retrieve in `units`
        /// @returns whether function was successful, check `popError()` for more info on `false`.
        bool getPosition(TimeUnit units, double *outPosition) const;

        /// Seek to a position in the track
        /// @param units       time units to set
        /// @param position position to seek to in `units`
        bool setPosition(TimeUnit units, uint64_t position);

        bool getSpec(AudioSpec *outSpec) const;
        bool getTargetSpec(AudioSpec *outTargetSpec) const;

        bool isEnded(bool *outEnded) const;

        bool getPCMFrameLength(uint64_t *outMaxFrames) const;
        bool getCursorPCMFrames(uint64_t *outCursor) const;
        bool getAvailableFrames(uint64_t *outFrames) const;
    private:
        struct Impl;
        Impl *m;
    };
}
