#pragma once
#include "../../AudioDevice.h"

namespace insound {

    class SdlAudioDevice final : public AudioDevice {
    public:
        SdlAudioDevice();
        ~SdlAudioDevice() override;

        bool open(int frequency,
            int sampleFrameBufferSize,
            AudioCallback callback,
            void *userdata) override;
        void close() override;
        void suspend() override;
        void resume() override;

        [[nodiscard]] bool isRunning() const override;
        [[nodiscard]] uint32_t id() const override;
        [[nodiscard]] const AudioSpec &spec() const override;
        [[nodiscard]] int bufferSize() const override;
    private:
        struct Impl;
        Impl *m;
    };

} // insound
