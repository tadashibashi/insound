#include "PCMSource.h"
#include "SoundBuffer.h"

namespace insound {
    PCMSource::PCMSource(Engine *engine, const SoundBuffer *buffer, uint32_t parentClock, const bool paused) :
        ISoundSource(engine, parentClock, paused),
        m_buffer(buffer), m_position(0)
    {
    }

    int PCMSource::readImpl(uint8_t *pcmPtr, int length)
    {
        const auto bytesLeft = (int)m_buffer->size() - (int)m_position;
        if (bytesLeft <= 0)
            return 0;

        const auto toRead = std::min((int)bytesLeft, length);

        // clear the buffer
        std::memset(pcmPtr, 0, length);

        // copy data to the buffer
        std::memcpy(pcmPtr, m_buffer->data() + m_position, toRead);

        m_position += toRead;
        return toRead;
    }

    bool PCMSource::hasEnded() const
    {
        return m_position >= m_buffer->size();
    }
}
