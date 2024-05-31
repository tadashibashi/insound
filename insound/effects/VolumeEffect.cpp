#include "VolumeEffect.h"

#include "insound/Error.h"

namespace insound {
    void VolumeEffect::process(float *input, float *output, int count)
    {
        for (int i = 0; i < count; i += 4)
        {
            output[i] = input[i] * m_volume;
            output[i+1] = input[i+1] * m_volume;
            output[i+2] = input[i+2] * m_volume;
            output[i+3] = input[i+3] * m_volume;
        }
    }

    void VolumeEffect::receiveParam(int index, float value)
    {
        switch(index)
        {
            case Param::Volume:
            {
                m_volume = value;
            } break;

            default:
            {
                pushError(Error::InvalidArg, "Unknown parameter index");
            } break;
        }
    }

    void VolumeEffect::volume(float value)
    {
        sendParam(Param::Volume, value);
    }
}
