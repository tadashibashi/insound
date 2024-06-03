#include "PanEffect.h"
#include "../Engine.h"
#include "../mathf.h"
#include "insound/Error.h"

namespace insound {
    void PanEffect::process(float *input, float *output, const int count)
    {
        for (int i = 0; i + 1 < count; i += 4)
        {
            const auto leftChan0  = (input[i + 1] * (1.f - m_right)) + (input[i] * m_left);
            const auto rightChan0 = (input[i] * (1.f - m_left)) + (input[i + 1] * m_right);
            const auto leftChan1  = (input[i + 3] * (1.f - m_right)) + (input[i + 2] * m_left);
            const auto rightChan1 = (input[i + 2] * (1.f - m_left)) + (input[i + 3] * m_right);
            // const auto leftChan2  = (input[i + 5] * (1.f - m_right)) + (input[i + 4] * m_left);
            // const auto rightChan2 = (input[i + 4] * (1.f - m_left)) + (input[i + 5] * m_right);
            // const auto leftChan3  = (input[i + 7] * (1.f - m_right)) + (input[i + 6] * m_left);
            // const auto rightChan3 = (input[i + 6] * (1.f - m_left)) + (input[i + 7] * m_right);

            output[i]     = leftChan0;
            output[i + 1] = rightChan0;
            output[i + 2] = leftChan1;
            output[i + 3] = rightChan1;
            // output[i + 4] = leftChan2;
            // output[i + 5] = rightChan2;
            // output[i + 6] = leftChan3;
            // output[i + 7] = rightChan3;
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
        sendFloat(Param::Left, mathf::clamp(value, 0, 1.f));
    }

    void PanEffect::right(float value)
    {
        sendFloat(Param::Right, mathf::clamp(value, 0, 1.f));
    }
} // insound
