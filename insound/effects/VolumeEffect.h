#pragma once
#include <insound/Effect.h>

namespace insound {
    class VolumeEffect : public Effect {
    public:
        explicit VolumeEffect(Engine *engine)
            : Effect(engine), m_volume(1.f)
        {
        }

        void process(float *input, float *output, int count) override;

        [[nodiscard]]
        float volume() const { return m_volume; }
        void volume(float value);

    private:
        void receiveParam(int index, float value) override;

        struct Param {
            enum Enum {
                Volume,
            };
        };
        float m_volume;
    };
}

