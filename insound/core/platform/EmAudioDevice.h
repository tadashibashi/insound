#pragma once

#ifdef __EMSCRIPTEN__
#include <insound/core/AudioDevice.h>

namespace insound {

    class EmAudioDevice final : public AudioDevice {
    public:
        EmAudioDevice();
        ~EmAudioDevice() override;

        bool open(int frequency,
            int sampleFrameBufferSize,
            AudioCallback audioCallback,
            void *userdata) override;
        void close() override;
        void suspend() override;
        void resume() override;
        [[nodiscard]] bool isRunning() const override;
        [[nodiscard]] uint32_t id() const override;
        [[nodiscard]] const AudioSpec &spec() const override;
        [[nodiscard]] int bufferSize() const override;
        [[nodiscard]] int getDefaultSampleRate() const override;

    public: // EmAudioDevice-specific functions (somewhat private since the AudioEngine only has a pointer to an AudioDevice)

        void read(const float **data, int length);

        /// Convenience function checking if `SharedArrayBuffer` and `WebAudio` are available.
        /// This can only be called from the main thread.
        [[nodiscard]]
        static bool isPlatformSupported();

        [[nodiscard]] bool isOpen() const override;
    private:
        struct Impl;
        Impl *m;
    };

} // insound
#endif
