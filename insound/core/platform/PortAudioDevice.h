#pragma once
#ifdef INSOUND_BACKEND_PORTAUDIO
#include <insound/core/AudioDevice.h>

namespace insound {

    class PortAudioDevice final : public AudioDevice {
    public:
        PortAudioDevice();
        ~PortAudioDevice() override;

        bool open(int frequency,
            int sampleFrameBufferSize,
            AudioCallback engineCallback,
            void *userdata) override;
        void close() override;
        void suspend() override;
        void resume() override;

        [[nodiscard]] bool isOpen() const override;
        [[nodiscard]] bool isRunning() const override;
        [[nodiscard]] uint32_t id() const override;
        [[nodiscard]] const AudioSpec &spec() const override;
        [[nodiscard]] int bufferSize() const override;
        [[nodiscard]] int getDefaultSampleRate() const override;

        void update() override;
    private:
        struct Impl;
        Impl *m;
    };

} // insound
#endif
