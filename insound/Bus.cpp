#include "Bus.h"

#include <SDL_audio.h>

namespace insound {
    Bus::Bus(Engine *engine, Bus *parent) : ISoundSource(engine, parent ? parent->clock() : 0),
        m_sources(), m_buffer(), m_parent(parent), m_panner()
    {
        m_panner = (PanEffect *)insertEffect(new PanEffect(engine), 0);
    }

    int Bus::readImpl(uint8_t *pcmPtr, int length)
    {
        // resize buffer if necessary
        if (m_buffer.size() * sizeof(float) < length)
        {
            m_buffer.resize(length / sizeof(float));
        }

        // clear buffer to 0s
        std::memset(m_buffer.data(), 0, m_buffer.size() * sizeof(float));

        // calculate mix
        for (const auto &source : m_sources)
        {
            const float *data;
            auto floatsToRead = source->read((const uint8_t **)&data, length, clock()) / sizeof(float);

            float *head = m_buffer.data();
            for (const float *ptr = data, *end = data + floatsToRead;
                ptr < end;
                ++ptr, ++head)
            {
                *head += *ptr;
            }
        }

        std::memcpy(pcmPtr, m_buffer.data(), length);
        return (int)(m_buffer.size() * sizeof(float));
    }

    void Bus::appendSource(ISoundSource *source)
    {
        m_sources.emplace_back(source);
    }

    bool Bus::removeSource(const ISoundSource *source)
    {
        for (auto it = m_sources.begin(); it != m_sources.end(); ++it)
        {
            if (*it == source)
            {
                m_sources.erase(it);
                return true;
            }
        }

        return false;
    }

    void Bus::update()
    {
        // if (!m_sources.empty())
        // {
            // erase any sounds that are no longer playing

            // this probably could be more efficient if source is tied to an engine,
            // and then calls a callback and sets flag when it is done, this flag is then checked to perform the remove-erase idiom
            // m_sources.erase(std::remove_if(m_sources.begin(), m_sources.end(), [](const ISoundSource *source) {
            //     return source->hasEnded();
            // }), m_sources.end());
        // }
    }

}
