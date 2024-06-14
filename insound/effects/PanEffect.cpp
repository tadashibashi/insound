#include "PanEffect.h"

#include <insound/Error.h>
#include <insound/util.h>

namespace insound {
    void PanEffect::process(float *input, float *output, const int count)
    {
        for (int i = 0; i + 1 < count; i += 4)
        {
            const auto leftChan0  = (input[i + 1] * (1.f - m_right)) + (input[i] * m_left);
            const auto rightChan0 = (input[i] * (1.f - m_left)) + (input[i + 1] * m_right);
            const auto leftChan1  = (input[i + 3] * (1.f - m_right)) + (input[i + 2] * m_left);
            const auto rightChan1 = (input[i + 2] * (1.f - m_left)) + (input[i + 3] * m_right);

            output[i]     = leftChan0;
            output[i + 1] = rightChan0;
            output[i + 2] = leftChan1;
            output[i + 3] = rightChan1;
        }
    }

    void PanEffect::receiveFloat(int index, float value)
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
                pushError(Result::InvalidArg, "PanEffect received unknown parameter index");
            } break;
        }
    }

    void PanEffect::left(const float value)
    {
        sendFloat(Param::Left, clamp(value, 0, 1.f));
    }

    void PanEffect::right(float value)
    {
        sendFloat(Param::Right, clamp(value, 0, 1.f));
    }
} // insound
