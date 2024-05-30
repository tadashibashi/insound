#pragma once
#include <insound/IEffect.h>

namespace insound {

    class PanEffect : public IEffect {
    public:
        explicit PanEffect(Engine *engine) : IEffect(engine), m_left(1.f), m_right(1.f) { }

        void process(float *input, float *output, int count) override;


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

        void receiveParam(int index, float value) override;

        float m_left, m_right;
    };

} // insound
