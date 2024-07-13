#pragma once
#include "AudioSpec.h"
#include "TimeUnit.h"

#include <string>

namespace insound {

    /// Decoder for streaming PCM data from various audio files
    /// Supported formats: WAV, FLAC, VORBIS, ADPCM, AIFF, MP3, TODO: GME, ModPlug backends
    class AudioDecoder {
    public:
        AudioDecoder();
        AudioDecoder(AudioDecoder &&other) noexcept;
        ~AudioDecoder();

        AudioDecoder &operator=(AudioDecoder &&other) noexcept;

        /// Open the file for streaming
        /// @param filepath     path to the file to open
        /// @param targetSpec   spec to convert the audio to
        /// @param inMemory     whether to copy entire file into memory, from which to stream;
        ///                         true:  copy file into memory and stream from memory
        ///                         false: stream directly from file (default)
        /// @note use `openConstMem` to stream from const memory where file data is already
        ///       loaded into memory
        bool open(const std::string &filepath, const AudioSpec &targetSpec, bool inMemory = false);

        /// Open a decoder from file data that has already been loaded into memory. It will stream
        /// the audio from this memory. Memory pointer must be valid until `AudioDecoder::close` is called.
        bool openConstMem(const uint8_t *data, size_t dataSize, const AudioSpec &targetSpec);

        /// Open a decoder from file data that has already been loaded into memory. Hand over ownership of this data
        /// to the AudioDecoder.
        /// on the `data` pointer.
        /// @param data pointer to the file data to read from
        /// @param dataSize size of the data buffer in bytes
        /// @param targetSpec spec to convert the audio to on output
        /// @param deallocator callback to cleanup the `data` pointer on close, default will call `std::free` on it.
        bool openMem(uint8_t *data, size_t dataSize, const AudioSpec &targetSpec, void(*deallocator)(void *data) = nullptr);

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
        bool postOpen(const AudioSpec &targetSpec);
        struct Impl;
        Impl *m;
    };
}
