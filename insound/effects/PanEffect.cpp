#include "PanEffect.h"
#include "../Engine.h"
#include "../mathf.h"
#include "insound/Error.h"

namespace insound {
    void PanEffect::process(float *samples, const int count)
    {
        for (int i = 0; i + 1 < count; i += 2)
        {
            const auto leftChan = (samples[i + 1] * (1.f - m_right)) + (samples[i] * m_left);
            const auto rightChan = (samples[i] * (1.f - m_left)) + (samples[i + 1] * m_right);

            samples[i] = leftChan;
            samples[i + 1] = rightChan;
        }
    }

    void PanEffect::receiveParam(int index, float value)
    {
        switch(index)
        {
            case Param::Left:
            {
                m_left = value;
            } break;

            case Param::Right:
            {
                m_right = value;
            } break;

            default:
            {
                pushError(Error::InvalidArg, "PanEffect received unknown parameter index");
            } break;
        }
    }

    void PanEffect::left(const float value)
    {
        setParam(Param::Left, mathf::clamp(value, 0, 1.f));
    }

    void PanEffect::right(float value)
    {
        setParam(Param::Right, mathf::clamp(value, 0, 1.f));
    }
} // insound