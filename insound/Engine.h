#pragma once
#include "SourceRefFwd.h"

#include <cstdint>
#include <memory>

namespace insound {
    struct AudioSpec;
    class Bus;
    struct Command;
    struct EngineCommand;
    class SoundBuffer;
    class SoundSource;
    class PCMSource;

    class Engine {
    public:
        Engine();
        ~Engine();

        bool open();
        void close();

        [[nodiscard]]
        bool isOpen() const;

        /// Play a sound
        /// @param buffer  buffer to play
        /// @param paused  whether to start sound paused; useful if you wish to modify the source before it plays
        /// @param looping whether to loop sound
        /// @param oneshot whether to release sound resources at end, (will not auto-release while looping is true)
        /// @param bus     bus this source should output to; left null the master bus will be used
        /// @param outPcmSource pointer to receive pcm source, nullable if you don't need it e.g. a oneshot sound
        bool playSound(const SoundBuffer *buffer, bool paused, bool looping, bool oneshot, SourceRef<Bus> &bus, SourceRef<PCMSource> *outPcmSource);
        bool playSound(const SoundBuffer *buffer, bool paused, bool looping, bool oneshot, SourceRef<PCMSource> *outPcmSource);

        /// Create a new bus to use in the mixing graph
        /// @param paused whether bus should start off paused on initialization
        /// @param output output bus to feed this bus to, if nullptr, the master Bus will be used
        bool createBus(bool paused, BusRef &output, BusRef *outBus);
        bool createBus(bool paused, BusRef *outBus);

        [[nodiscard]]
        bool deviceID(uint32_t *outDeviceID) const;

        /// Get audio spec
        bool getSpec(AudioSpec *outSpec) const;

        /// Get the size of audio buffer in bytes
        bool getBufferSize(uint32_t *outSize) const;

        /// Get the master bus
        bool getMasterBus(SourceRef<Bus> *outbus) const;

        /// Push a command to be deferred until `update` is called
        bool pushCommand(const Command &command);

        /// Push a command that will immediately be processed the next audio buffer.
        /// This is for commands that are sample clock-sensitive.
        bool pushImmediateCommand(const Command &command);

        /// Pause the audio device
        /// @returns whether function succeeded, check `popError()` for details
        bool setPaused(bool value);

        /// Get if the device is paused
        bool getPaused(bool *outValue) const;

        bool update();

        /// Flag a SoundSource for removal from the mix graph, it becomes invalidated and will no longer function
        /// @returns whether function succeeded; check `popError()` for details
        bool flagDiscard(SoundSource *source);

        /// Check if a SoundSource is currently valid
        [[nodiscard]]
        bool isSourceValid(const std::shared_ptr<SoundSource> &source) const;
        /// Check if a SoundSource is currently valid
        [[nodiscard]]
        bool isSourceValid(const SoundSource *source) const;

        struct Impl;
    private:
        void applyCommand(const EngineCommand &command);
        Impl *m;
    };
}
