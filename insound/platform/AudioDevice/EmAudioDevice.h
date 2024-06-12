#pragma once
#ifdef __EMSCRIPTEN__
#include "AudioDevice.h"

namespace insound {

    class EmAudioDevice final : public AudioDevice {
    public:
        EmAudioDevice();
        ~EmAudioDevice() override;

        bool open(int frequency,
            int sampleFrameBufferSize,
            void (*audioCallback)(void *userdata, std::vector<uint8_t> *stream),
            void *userdata) override;
        void close() override;
        void suspend() override;
        void resume() override;

        [[nodiscard]]
        bool isRunning() const override;

        [[nodiscard]]
        static bool isPlatformSupported();

        [[nodiscard]]
        uint32_t id() const override;

        [[nodiscard]]
        const AudioSpec &spec() const override;

        void setAudioWorkletNode(void *node);

        void read(const float **data, int length);

        [[nodiscard]]
        int bufferSize() const override;
    private:
        void mixCallback();
        struct Impl;
        Impl *m;
    };

} // insound
#endif