#pragma once
#include "AudioSpec.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace insound {
    /// Abstraction over an audio i/o backend
    class AudioDevice {
    public:
        /// Create a new instance of the backend audio device
        static AudioDevice *create();

        /// Delete an instance of the backend audio device
        static void destroy(AudioDevice *device);

        /// Open audio device
        /// @param frequency         requested sample rate for the output device
        /// @param sampleFrameBuffer number of sample frames per buffer
        /// @param audioCallback     callback used in the audio thread (
        ///                             `userdata` = user-provided ptr passed to callback,
        ///                             `buffer` = buffer to fill, may be swapped with internal buffer as long as it maintains its size)
        /// @param userdata          userdata to pass to the audio callback
        /// @returns whether device was successfully opened
        virtual bool open(
            int frequency,
            int sampleFrameBuffer,
            void (*audioCallback)(void *userdata, std::vector<uint8_t> *buffer), ///< buffer to fill, may be swapped with internal buffer as long as it maintains its size
            void *userdata) = 0;

        /// Close the audio device, should block
        virtual void close() = 0;
        /// Suspend the audio device, no output
        virtual void suspend() = 0;
        /// Resume the audio device
        virtual void resume() = 0;

        /// Whether device is running (not suspended)
        [[nodiscard]]
        virtual bool isRunning() const = 0;

        /// Whether device was opened
        [[nodiscard]]
        bool isOpen() const { return id() != 0; }

        /// Device ID, 0 means that the device has not been initialized
        [[nodiscard]]
        virtual uint32_t id() const = 0;

#ifdef INSOUND_THREADING
        /// Mix thread lock guard
        [[nodiscard]]
        std::lock_guard<std::recursive_mutex> mixLockGuard();
#endif
        /// Audio spec of the output device
        [[nodiscard]]
        virtual const AudioSpec &spec() const = 0;

        /// Target buffer size of output device (may pass smaller size to audio callback)
        [[nodiscard]]
        virtual int bufferSize() const = 0;
    protected:
        std::recursive_mutex m_mixMutex{};
        virtual ~AudioDevice() = default;
    };
}
