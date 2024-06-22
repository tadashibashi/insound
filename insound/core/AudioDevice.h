#pragma once
#include "AudioSpec.h"
#include "AlignedVector.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace insound {
    /// Audio callback that the AudioDevice uses to retrieves master output data from the mix graph to send to the audio card.
    /// @param userdata    custom data passed during AudioDevice::open made available to the callback
    /// @param buffer      audio buffer to write to (or swap, given that the sizes of both are equal)
    using AudioCallback = void (*)(void *userdata, AlignedVector<uint8_t, 16> *buffer);

    /// Abstraction over an audio i/o backend
    class AudioDevice {
    public:
        /// Create a new instance of the backend audio device.
        /// The platform is automatically selected via defines.
        static AudioDevice *create();

        /// Delete an instance of the backend audio device
        static void destroy(AudioDevice *device);

        /// Open audio device
        /// @param frequency         requested sample rate for the output device
        /// @param sampleFrameBuffer number of sample frames per buffer
        /// @param audioCallback     callback that the AudioDevice uses to retrieve mix output data to send to the audio card
        /// @param userdata          userdata to pass to the audio callback
        /// @returns whether device was successfully opened
        virtual bool open(
            int frequency,
            int sampleFrameBuffer,
            AudioCallback audioCallback, ///< buffer to fill, may be swapped with internal buffer as long as it maintains its size
            void *userdata) = 0;

        /// Close the audio device, should block. Safe to call if already closed.
        virtual void close() = 0;

        /// Suspend the audio device, effectively pausing it.
        virtual void suspend() = 0;

        /// Resume the audio device, unpausing it, if suspended.
        virtual void resume() = 0;

        /// Whether this device is running (not suspended)
        [[nodiscard]]
        virtual bool isRunning() const = 0;

        /// Whether this device was opened
        [[nodiscard]]
        virtual bool isOpen() const = 0;

        /// Device ID, 0 means that the device has not been initialized
        [[nodiscard]]
        virtual uint32_t id() const = 0;

        /// Audio spec of the output device
        [[nodiscard]]
        virtual const AudioSpec &spec() const = 0;

        /// Use as a last resort to sync with the mix thread. If you're using this externally, you're probably bypassing the command queue
        std::lock_guard<std::recursive_mutex> mixLockGuard() const { return std::lock_guard(m_mixMutex); }

        /// Target buffer size of output device (may pass smaller size to audio callback)
        [[nodiscard]]
        virtual int bufferSize() const = 0;
    protected:
        mutable std::recursive_mutex m_mixMutex;
        virtual ~AudioDevice() = default;
    };
}
