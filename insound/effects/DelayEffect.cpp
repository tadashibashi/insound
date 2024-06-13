#include "DelayEffect.h"

#include "insound/Engine.h"
#include "insound/Error.h"


insound::DelayEffect::DelayEffect() :
    m_delayTime(48000), m_feedback(), m_wet(.5f), m_delayHead(0)
{
}

void insound::DelayEffect::process(float *input, float *output, int count)
{
    const auto bufSize = m_buffer.size();
    const auto dry = 1.f - m_wet;
    const auto wet = m_wet;
    const auto feedback = m_feedback;

    // write to output (add from delay buffer), read from input and fill buffer
    for (int i = 0; i < count; i += 4)
    {
        output[i] = input[i] * dry + m_buffer[(m_delayHead + i) % bufSize] * wet;
        m_buffer[(m_delayHead + i) % bufSize] = input[i] * feedback;

        output[i + 1] = input[i + 1] * dry + m_buffer[(m_delayHead + i + 1) % bufSize] * wet;
        m_buffer[(m_delayHead + i + 1) % bufSize] = input[i+1] * feedback;

        output[i + 2] = input[i + 2] * dry + m_buffer[(m_delayHead + i + 2) % bufSize] * wet;
        m_buffer[(m_delayHead + i + 2) % bufSize] = input[i+2] * feedback;

        output[i + 3] = input[i + 3] * dry + m_buffer[(m_delayHead + i + 3) % bufSize] * wet;
        m_buffer[(m_delayHead + i + 3) % bufSize] = input[i+3] * feedback;
    }

    // update delay head
    m_delayHead = (m_delayHead + count) % bufSize;
}

void insound::DelayEffect::delayTime(uint32_t samples)
{
    sendInt(Param::DelayTime, (int)samples); // TODO: enforce a maximum?
}

void insound::DelayEffect::feedback(const float value)
{
    sendFloat(Param::Feedback, value);
}

void insound::DelayEffect::wetDry(float value)
{
    sendFloat(Param::Wet, value);
}

void insound::DelayEffect::receiveFloat(int index, float value)
{
    switch(index)
    {
        case Param::Feedback:
        {
            m_feedback = value;
        } break;

        case Param::Wet:
        {
            m_wet = value;
        } break;

        default:
        {
            pushError(Result::InvalidArg, "Unknown parameter index passed to DelayEffect::receiveFloat");
        } break;
    }
}

void insound::DelayEffect::receiveInt(int index, int value)
{
    switch(index)
    {
        case Param::DelayTime:
        {
            const auto convertedValue = (uint32_t)value;
            if (m_delayTime != convertedValue) // sample frames
            {
                m_buffer.resize(convertedValue * 2); // TODO: may want to smoothen any artifacts from resizing buffer
                m_delayTime = convertedValue;
            }
        } break;

        default:
        {
            pushError(Result::InvalidArg, "Unknown parameter index passed to DelayEffect::receiveInt");
        } break;
    }

}
