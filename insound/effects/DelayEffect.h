#pragma once

#include "../Effect.h"

#include <cstdint>
#include <vector>

namespace insound {
    class DelayEffect: public Effect {
    public:
        DelayEffect(Engine *engine, uint32_t delayTime, float wet, float feedback);
        ~DelayEffect() override = default;

        void process(float *input, float *output, int count) override;

        void delayTime(uint32_t samples);

        [[nodiscard]]
        uint32_t delayTime() const { return m_delayTime; }

        void feedback(float value);

        [[nodiscard]]
        float feedback() const { return m_feedback; }

        void wet(float value);

        [[nodiscard]]
        float wet() const { return m_wet; }

    protected:
        struct Param {
            enum Enum {
                DelayTime,
                Feedback,
                Wet,
            };
        };
        void receiveParam(int index, float value) override;

    private:
        std::vector<float> m_buffer;
        uint32_t m_delayTime; // delay time in sample frames
        float m_feedback;
        float m_wet;

        uint32_t m_delayHead;
    };
}
