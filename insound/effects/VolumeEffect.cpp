#include "VolumeEffect.h"

#include "insound/Error.h"

namespace insound {
    void VolumeEffect::process(float *input, float *output, int count)
    {
        const auto volume = m_volume;
        for (int i = 0; i < count; i += 4)
        {
            output[i] = input[i] * volume;
            output[i+1] = input[i+1] * volume;
            output[i+2] = input[i+2] * volume;
            output[i+3] = input[i+3] * volume;
        }
    }

    void VolumeEffect::receiveFloat(int index, float value)
    {
        switch(index)
        {
            case Param::Volume:
            {
                m_volume = value;
            } break;

            default:
            {
                pushError(Result::InvalidArg, "Unknown parameter index");
            } break;
        }
    }

    void VolumeEffect::volume(float value)
    {
        sendFloat(Param::Volume, value);
    }
}
