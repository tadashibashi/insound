#pragma once
#include <insound/core/lib.h>

#if INSOUND_TARGET_IOS
#include <insound/core/AudioDevice.h>

namespace insound {
    class iOSAudioDevice : public AudioDevice {
    public:
        iOSAudioDevice();
        ~iOSAudioDevice() override;

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
        [[nodiscard]] bool isOpen() const override;
    private:
        struct Impl;
        Impl *m;
    };
}
#endif
