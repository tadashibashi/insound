#pragma once
#include "insound/IEffect.h"

namespace insound {
    class VolumeEffect : public IEffect {
    public:
        explicit VolumeEffect(Engine *engine)
            : IEffect(engine), m_volume(1.f)
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

