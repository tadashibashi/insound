#pragma once
#include <insound/AudioDevice.h>

namespace insound {

    class SdlAudioDevice final : public AudioDevice {
    public:
        SdlAudioDevice();
        ~SdlAudioDevice() override;

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
        uint32_t id() const override;

        [[nodiscard]]
        const AudioSpec &spec() const override;

        [[nodiscard]]
        int bufferSize() const override;
    private:
#ifdef INSOUND_THREADING
        void mixCallback();
#else
        static void audioCallback(void *userdata, uint8_t *stream, int length);
#endif
        struct Impl;
        Impl *m;
    };

} // insound
