#pragma once
#include "ISoundSource.h"

#include <cstdint>

namespace insound {
    class Bus;
    class SoundBuffer;

    class PCMSource final : public ISoundSource {
    public:
        explicit PCMSource(Engine *engine, const SoundBuffer *buffer, uint32_t parentClock, bool paused = false);

        /// Position offset within current sound source in bytes
        [[nodiscard]]
        uint32_t position() const { return m_position; }

        [[nodiscard]]
        bool hasEnded() const;

    private:
        /// Get the current pointer position
        /// @returns the amount of bytes available or length arg, whichever is smaller
        int readImpl(uint8_t **pcmPtr, int length) override;
        const SoundBuffer *m_buffer;
        uint32_t m_position;
    };
}
