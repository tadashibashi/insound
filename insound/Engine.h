#pragma once
#include "Command.h"
#include <cstdint>

namespace insound {
    struct AudioSpec;
    class Bus;
    class SoundBuffer;
    class PCMSource;

    template <typename T>
    class SourceHandle;

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
        SourceHandle<PCMSource> playSound(const SoundBuffer *buffer, bool paused = false, bool looping = false, bool oneshot = false, Bus *bus = nullptr);

        [[nodiscard]]
        uint32_t deviceID() const;

        /// Get audio spec
        bool getSpec(AudioSpec *outSpec) const;

        /// Get the size of audio buffer in bytes
        [[nodiscard]]
        bool getBufferSize(uint32_t *outSize) const;

        bool getMasterBus(Bus **bus) const;

        void pushCommand(const Command &command);
        void pushImmediateCommand(const Command &command);

        void update();

        void flagDiscard(SoundSource *source);

        [[nodiscard]]
        bool isSourceValid(SoundSource *source);

        struct Impl;
    private:
        Impl *m;
    };
}
