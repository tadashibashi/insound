#include "DelayEffect.h"

#include "insound/Engine.h"
#include "insound/Error.h"


insound::DelayEffect::DelayEffect(Engine *engine, uint32_t delayTime, float wet, float feedback): Effect(engine),
    m_delayTime(delayTime), m_feedback(feedback), m_wet(wet), m_delayHead(0)
{
    m_buffer.resize(delayTime * 2);
    std::memset(m_buffer.data(), 0, m_buffer.size() * sizeof(float));
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
    sendParam(Param::DelayTime, (float)samples);
}

void insound::DelayEffect::feedback(const float value)
{
    sendParam(Param::Feedback, value);
}

void insound::DelayEffect::wet(float value)
{
    sendParam(Param::Wet, value);
}

void insound::DelayEffect::receiveParam(int index, float value)
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
            pushError(Error::InvalidArg, "Unknown parameter index passed to DelayEffect");
        } break;
    }
}
