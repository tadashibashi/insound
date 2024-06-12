#pragma once
#include <insound/AudioSpec.h>

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
        virtual bool open(
            int frequency,
            int sampleFrameBuffer,
            void (*audioCallback)(void *userdata, std::vector<uint8_t> *buffer), ///< buffer to fill, may be swapped with internal buffer as long as it maintains its size
            void *userdata) = 0;
        virtual void close() = 0;
        virtual void suspend() = 0;
        virtual void resume() = 0;

        [[nodiscard]]
        virtual bool isRunning() const = 0;

        [[nodiscard]]
        bool isOpen() const { return id() != 0; }

        /// Device ID, 0 means that the device has not been initialized
        [[nodiscard]]
        virtual uint32_t id() const = 0;

        [[nodiscard]]
        std::lock_guard<std::recursive_mutex> mixLockGuard();

        [[nodiscard]]
        virtual const AudioSpec &spec() const = 0;

        [[nodiscard]]
        virtual int bufferSize() const = 0;
    protected:
        std::recursive_mutex m_mixMutex{};
        virtual ~AudioDevice() = default;
    };
}
