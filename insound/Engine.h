#pragma once
#include "Command.h"
#include <cstdint>
struct SDL_AudioSpec;

namespace insound {
    class Bus;
    class SoundBuffer;
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
        /// @param buffer buffer to play
        /// @param paused whether to start sound paused; useful if you wish to modify the source before it plays
        /// @param bus    bus this source should output to; left null the master bus will be used
        PCMSource *playSound(const SoundBuffer *buffer, bool paused = false, Bus *bus = nullptr);

        [[nodiscard]]
        uint32_t deviceID() const;

        /// TODO: create our own spec object or make individual getters
        bool getSpec(SDL_AudioSpec *outSpec) const;

        bool getMasterBus(Bus **bus);

        void pushCommand(const Command &command);
        void pushImmediateCommand(const Command &command);

        void update();

        struct Impl;
    private:
        Impl *m;
    };
}
