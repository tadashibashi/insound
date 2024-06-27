#pragma once

#ifdef __ANDROID__
#include "../../AudioDevice.h"

namespace insound {

    class AAudioDevice final : public AudioDevice {
    public:
        AAudioDevice();
        ~AAudioDevice() override;

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
        [[nodiscard]] bool isOpen() const override;
    private:
        struct Impl;
        Impl *m;
    };

} // insound
#endif
