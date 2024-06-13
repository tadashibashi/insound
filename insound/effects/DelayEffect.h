#pragma once

#include "../Effect.h"

#include <cstdint>
#include <vector>

namespace insound {
    class DelayEffect: public Effect {
    public:
        DelayEffect();
        ~DelayEffect() override = default;

        bool init(uint32_t delayTime, float wet, float feedback)
        {
            if (!Effect::init())
                return false;

            m_delayTime = delayTime;
            m_wet = wet;
            m_feedback = feedback;

            m_buffer.resize(delayTime * 2);
            std::memset(m_buffer.data(), 0, m_buffer.size() * sizeof(float));

            return true;
        }

        void process(float *input, float *output, int count) override;

        /// Set the delay time in sample frames, (use engine spec to find sample rate)
        void delayTime(uint32_t samples);

        /// Get the current delay time in sample frames
        [[nodiscard]]
        uint32_t delayTime() const { return m_delayTime; }

        /// Set the input feedback percentage, where 0 = 0% through 1.f = 100%
        void feedback(float value);

        /// Get the input feedback percentage, where 0 = 0% through 1.f = 100%
        [[nodiscard]]
        float feedback() const { return m_feedback; }

        /// Set the wet and dry signal percentages at once, where the value of the wet signal is equal to `value`,
        /// and dry is `1.f - value`.
        /// For example, 0 results in 0% wet signal (no effect applied with 100% dry
        /// signal), and 1.f results in 100% of the wet signal and 0% of the dry.
        void wetDry(float value);

        /// Get the wet and dry signal percentages at once, where the value of the wet signal is equal to `value`,
        /// and dry is `1.f - value`.
        /// For example, 0 results in 0% wet signal (no effect applied with 100% dry
        /// signal), and 1.f results in 100% of the wet signal and 0% of the dry.
        [[nodiscard]]
        float wetDry() const { return m_wet; }

    protected:
        struct Param {
            enum Enum {
                DelayTime,
                Feedback,
                Wet,
            };
        };
        void receiveFloat(int index, float value) override;
        void receiveInt(int index, int value) override;

    private:
        std::vector<float> m_buffer;
        uint32_t m_delayTime; // delay time in sample frames
        float m_feedback;
        float m_wet;

        uint32_t m_delayHead;
    };
}
