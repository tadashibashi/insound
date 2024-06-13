#pragma once
#include <insound/Effect.h>

namespace insound {

    class PanEffect : public Effect {
    public:
        PanEffect() : m_left(1.f), m_right(1.f) { }
        PanEffect(float left, float right) : m_left(left), m_right(right) { }

        void process(float *input, float *output, int count) override;

        bool init()
        {
            if (!Effect::init())
                return false;

            m_left = 1.f;
            m_right = 1.f;

            return true;
        }


        // ----- getters / setters -----

        void left(float value);
        void right(float value);

        [[nodiscard]]
        auto left() const { return m_left; }
        [[nodiscard]]
        auto right() const { return m_right; }

    private:
        struct Param {
            enum Enum {
                Left,
                Right
            };
        };

        void receiveFloat(int index, float value) override;

        float m_left, m_right;
    };

} // insound
