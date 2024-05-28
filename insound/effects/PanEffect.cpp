#include "PanEffect.h"
#include "../Engine.h"
#include "../mathf.h"
#include "insound/Error.h"

namespace insound {
    void PanEffect::process(float *input, float *output, const int count)
    {
        for (int i = 0; i + 1 < count; i += 2)
        {
            const auto leftChan = (input[i + 1] * (1.f - m_right)) + (input[i] * m_left);
            const auto rightChan = (input[i] * (1.f - m_left)) + (input[i + 1] * m_right);

            output[i]     = leftChan;
            output[i + 1] = rightChan;
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
        sendParam(Param::Left, mathf::clamp(value, 0, 1.f));
    }

    void PanEffect::right(float value)
    {
        sendParam(Param::Right, mathf::clamp(value, 0, 1.f));
    }
} // insound