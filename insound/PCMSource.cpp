#include "PCMSource.h"

#include "Command.h"
#include "Engine.h"
#include "Error.h"
#include "SoundBuffer.h"

namespace insound {
    PCMSource::PCMSource(Engine *engine, const SoundBuffer *buffer, uint32_t parentClock, const bool paused,
        const bool looping, const bool oneShot) : SoundSource(engine, parentClock, paused),
                                                  m_buffer(buffer), m_position(0), m_isLooping(looping),
                                                  m_isOneShot(oneShot), m_speed(1.f)
    {
    }

    bool PCMSource::setPosition(const float value)
    {
        if (detail::popSystemError().code == Result::InvalidHandle)
        {
            pushError(Result::InvalidHandle);
            return false;
        }

        engine()->pushImmediateCommand(Command::makePCMSourceSetPosition(this, value));
        return true;
    }

    bool PCMSource::getSpeed(float *outSpeed) const
    {
        if (detail::popSystemError().code == Result::InvalidHandle)
        {
            pushError(Result::InvalidHandle);
            return false;
        }

        if (outSpeed)
        {
            *outSpeed = m_speed;
        }

        return true;
    }

    bool PCMSource::setSpeed(float value)
    {
        if (detail::popSystemError().code == Result::InvalidHandle)
        {
            pushError(Result::InvalidHandle);
            return false;
        }

        engine()->pushCommand(Command::makePCMSourceSetSpeed(this, value));
        return true;
    }

    void PCMSource::applyCommand(const PCMSourceCommand & command)
    {
        switch(command.type)
        {
            case PCMSourceCommand::SetPosition:
            {
                m_position = command.setposition.position;
            } break;

            case PCMSourceCommand::SetSpeed:
            {
                m_speed = command.setspeed.speed;
            } break;

            default:
            {
                pushError(Result::InvalidArg, "Unknown pcm source command type");
            } break;
        }
    }

    int PCMSource::readImpl(uint8_t *output, int length)
    {
        const auto sampleSize = m_buffer->size() / sizeof(float);
        const auto frameSize = sampleSize / 2;
        const auto sampleLength = length / sizeof(float);
        const auto frameLength = sampleLength / 2;
        const int framesToRead = m_isLooping ? (int)frameLength : std::min<int>((int)frameLength, (int)frameSize - (int)std::ceil(m_position));

        const auto buffer = (float *)m_buffer->data();

        // clear the write buffer
        std::memset(output, 0, length);

        if (framesToRead <= 0)
        {
            return length;
        }

        int i = 0;
        for (; i < framesToRead - 4; i += 4) // visit each frame (2 floats)
        {
            // m_position is the frame position
            // current sample position
            const float framef = std::fmodf(((float)m_position + ((float)i * m_speed)), (float)frameSize);
            const float t = framef - std::floorf(framef);

            // Get indices and next indices to interpolate between
            const int indexL = (int)framef * 2;                        // left sample  0
            const int nextIndexL = (indexL + 2) % (int)sampleSize;
            const int indexR = ((int)framef * 2 + 1);                  // right sample 0
            const int nextIndexR = (indexR + 2) % (int)sampleSize;

            const int indexL1 = (int)(framef + m_speed) * 2;           // left sample  1
            const int nextIndexL1 = (indexL1 + 2) % (int)sampleSize;
            const int indexR1 = ((int)(framef + m_speed) * 2 + 1);     // right sample 1
            const int nextIndexR1 = (indexR1 + 2) % (int)sampleSize;

            const int indexL2 = (int)(framef + m_speed * 2) * 2;       // left sample  2
            const int nextIndexL2 = (indexL2 + 2) % (int)sampleSize;
            const int indexR2 = ((int)(framef + m_speed * 2) * 2 + 1); // right sample 2
            const int nextIndexR2 = (indexR2 + 2) % (int)sampleSize;

            const int indexL3 = (int)(framef + m_speed * 3) * 2;       // left sample  3
            const int nextIndexL3 = (indexL3 + 2) % (int)sampleSize;
            const int indexR3 = ((int)(framef + m_speed * 3) * 2 + 1); // right sample 3
            const int nextIndexR3 = (indexR3 + 2) % (int)sampleSize;

            const auto tMinus1 = 1.f-t;

            ((float *)output)[i * 2] = buffer[indexL] * (t) + buffer[nextIndexL] * (tMinus1);
            ((float *)output)[i * 2 + 1] = buffer[indexR] * (t) + buffer[nextIndexR] * (tMinus1);
            ((float *)output)[i * 2 + 2] = buffer[indexL1] * (t) + buffer[nextIndexL1] * (tMinus1);
            ((float *)output)[i * 2 + 3] = buffer[indexR1] * (t) + buffer[nextIndexR1] * (tMinus1);
            ((float *)output)[i * 2 + 4] = buffer[indexL2] * (t) + buffer[nextIndexL2] * (tMinus1);
            ((float *)output)[i * 2 + 5] = buffer[indexR2] * (t) + buffer[nextIndexR2] * (tMinus1);
            ((float *)output)[i * 2 + 6] = buffer[indexL3] * (t) + buffer[nextIndexL3] * (tMinus1);
            ((float *)output)[i * 2 + 7] = buffer[indexR3] * (t) + buffer[nextIndexR3] * (tMinus1);
        }
        for (; i < framesToRead; ++i) // catch leftovers
        {
            // m_position is the frame position
            // current sample position
            const float framef = std::fmodf(((float)m_position + ((float)i * m_speed)), (float)frameSize);
            const float t = framef - std::floorf(framef);

            const int indexL = (int)framef * 2; // left sample index
            const int nextIndexL = (indexL + 2) % (int)sampleSize;
            const int indexR = ((int)framef * 2 + 1); // right sample index
            const int nextIndexR = (indexR + 2) % (int)sampleSize;

            const auto tMinus1 = 1.f-t;

            ((float *)output)[i * 2] = buffer[indexL] * (t) + buffer[nextIndexL] * (tMinus1);
            ((float *)output)[i * 2 + 1] = buffer[indexR] * (t) + buffer[nextIndexR] * (tMinus1);
        }

        if (m_isLooping)
        {
            m_position = std::fmodf(m_position + (float)framesToRead * m_speed, (float)frameSize);
        }
        else
        {
            m_position += (float)framesToRead * m_speed;
            if (m_isOneShot && m_position >= (float)frameSize)
            {
                release();
            }
        }

        return (int)framesToRead * (int)sizeof(float) * 2;
    }

    bool PCMSource::hasEnded() const
    {
        return m_position >= (float)m_buffer->size() / (sizeof(float) * 2.f);
    }

    bool PCMSource::getPosition(float *outPosition) const
    {
        if (detail::popSystemError().code == Result::InvalidHandle)
        {
            pushError(Result::InvalidHandle);
            return false;
        }

        if (outPosition)
            *outPosition = m_position;

        return true;
    }
}
