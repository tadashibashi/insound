#pragma once
#include "../Effect.h"

namespace insound {
    class VolumeEffect : public Effect {
    public:
        VolumeEffect()
            : m_volume(1.f)
        {
        }

        VolumeEffect(VolumeEffect &&other) noexcept;

        bool init(float volume = 1.f)
        {
            m_volume = volume;

            return true;
        }

        explicit VolumeEffect(float volume) : m_volume(volume) { }

        bool process(const float *input, float *output, int count) override;

        [[nodiscard]]
        float volume() const { return m_volume; }
        void volume(float value);

    private:
        void receiveFloat(int index, float value) override;

        struct Param {
            enum Enum {
                Volume,
            };
        };
        float m_volume;
    };
}

