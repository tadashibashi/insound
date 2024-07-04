#pragma once
#ifdef INSOUND_BACKEND_SDL3
#include "../../AudioDevice.h"

namespace insound {

    class Sdl3AudioDevice final : public AudioDevice {
    public:
        Sdl3AudioDevice();
        ~Sdl3AudioDevice() override;

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
    private:
        struct Impl;
        Impl *m;
    };

} // insound
#endif
