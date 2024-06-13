#pragma once
#include "Source.h"

#include <cstdint>

namespace insound {
    class Bus;
    class SoundBuffer;

    class PCMSource final : public Source {
    public:
        PCMSource();

        bool init(Engine *engine, const SoundBuffer *buffer, uint32_t parentClock, bool paused = false,
            bool looping = false, bool oneShot = false);

        bool getEnded(bool *outEnded) const;

        bool getPosition(float *outPosition) const;
        bool setPosition(float value);

        bool getSpeed(float *outSpeed) const;
        bool setSpeed(float value);

        bool getLooping(bool *outLooping) const;
        bool setLooping(bool value);

        bool getOneshot(bool *outOneshot) const;
        bool setOneshot(bool value);

    private:
        friend class Engine;
        void applyCommand(const struct PCMSourceCommand &command);

        /// Get the current pointer position
        /// @returns the amount of bytes available or length arg, whichever is smaller
        int readImpl(uint8_t *output, int length) override;
        const SoundBuffer *m_buffer;
        float m_position;
        bool m_isLooping;
        bool m_isOneShot;
        float m_speed;
    };
}
