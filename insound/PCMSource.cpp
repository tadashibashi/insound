#include "PCMSource.h"

#include "Engine.h"
#include "Error.h"
#include "SoundBuffer.h"

namespace insound {
    PCMSource::PCMSource(Engine *engine, const SoundBuffer *buffer, uint32_t parentClock, const bool paused,
        const bool looping, const bool oneShot) : SoundSource(engine, parentClock, paused),
                                                  m_buffer(buffer), m_position(0), m_isLooping(looping),
                                                  m_isOneShot(oneShot)
    {
    }

    void PCMSource::position(const uint32_t value)
    {
        engine()->pushImmediateCommand(Command::makePCMSourceSetPosition(this, value));
    }

    void PCMSource::applyPCMSourceCommand(const Command &command)
    {
        if (command.data.pcmsource.type == PCMSourceCommandType::SetPosition)
        {
            m_position = command.data.pcmsource.data.setposition.position;
        }
        else
        {
            pushError(Error::InvalidArg, "Unknown pcm source command type");
        }
    }

    int PCMSource::readImpl(uint8_t *pcmPtr, int length)
    {
        // clear the buffer
        std::memset(pcmPtr, 0, length);

        const auto bufferSize = m_buffer->size();

        if (m_isLooping)
        {
            int bytesProcessed = 0;
            while (bytesProcessed < length)
            {
                int bytesLeftInBuffer = (int)(bufferSize - m_position);
                int bytesToReadThisIteration = std::min(bytesLeftInBuffer, length - bytesProcessed);

                std::memcpy(pcmPtr + bytesProcessed, m_buffer->data() + m_position, bytesToReadThisIteration);

                bytesProcessed += bytesToReadThisIteration;

                m_position += bytesToReadThisIteration;

                while(m_position >= bufferSize)
                {
                    m_position -= bufferSize;
                }
            }

            return length;
        }

        const auto bytesLeft = (int)bufferSize - (int)m_position;
        if (bytesLeft <= 0)
            return 0;

        const auto toRead = std::min((int)bytesLeft, length);

        // copy data to the buffer
        std::memcpy(pcmPtr, m_buffer->data() + m_position, toRead);

        m_position += toRead;

        if (m_isOneShot && m_position >= bufferSize)
        {
            release();
        }

        return toRead;
    }

    bool PCMSource::hasEnded() const
    {
        return m_position >= m_buffer->size();
    }
}
