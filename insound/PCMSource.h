#pragma once
#include "SoundSource.h"

#include <cstdint>

namespace insound {
    class Bus;
    class SoundBuffer;

    class PCMSource final : public SoundSource {
    public:
        explicit PCMSource(Engine *engine, const SoundBuffer *buffer, uint32_t parentClock, bool paused = false, bool looping = false, bool oneShot = false);

        [[nodiscard]]
        bool hasEnded() const;

        [[nodiscard]]
        uint32_t position() const { return m_position; }
        void position(uint32_t value);

    private:
        friend class Engine;
        void applyPCMSourceCommand(const struct Command &command);

        /// Get the current pointer position
        /// @returns the amount of bytes available or length arg, whichever is smaller
        int readImpl(uint8_t *pcmPtr, int length) override;
        const SoundBuffer *m_buffer;
        uint32_t m_position;
        bool m_isLooping;
        bool m_isOneShot;
    };
}
