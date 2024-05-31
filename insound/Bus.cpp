#include "Bus.h"

#include <SDL_audio.h>

#include "Engine.h"
#include "Error.h"

namespace insound {
    Bus::Bus(Engine *engine, Bus *parent) : SoundSource(engine, parent ? parent->clock() : 0),
        m_sources(), m_buffer(), m_parent(parent), m_panner()
    {
        m_panner = (PanEffect *)insertEffect(new PanEffect(engine), 0);
    }

    void Bus::updateParentClock(uint32_t parentClock)
    {
        SoundSource::updateParentClock(parentClock);

        // Recursively update all sub-sources
        const auto curClock = clock();
        for (auto &source : m_sources)
        {
            source->updateParentClock(curClock);
        }
    }

    void Bus::processRemovals()
    {
        // grab master bus ref
        Bus *masterBus;
        if (!engine()->getMasterBus(&masterBus))
        {
            return;
        }

        // erase-remove idiom on all sound sources with discard flagged true
        m_sources.erase(std::remove_if(m_sources.begin(), m_sources.end(), [masterBus] (SoundSource *source) {
            auto shouldDiscard = source->shouldDiscard();

            if (auto bus = dynamic_cast<Bus *>(source))
            {
                bus->processRemovals(); // if graph is huge, this recursive call could be a problem...

                if (shouldDiscard) // if this bus is flagged for discarding
                {
                    for (auto &subSource : bus->m_sources)
                    {
                        masterBus->appendSource(subSource);
                        if (auto subBus = dynamic_cast<Bus *>(subSource)) // set each subBus's new parent
                        {
                            subBus->m_parent = masterBus;
                        }
                    }
                }
            }

            if (shouldDiscard)
            {
                delete source;
            }

            return shouldDiscard;
        }), m_sources.end());
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
            auto floatsToRead = source->read((const uint8_t **)&data, length) / sizeof(float);

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

    void Bus::appendSource(SoundSource *source)
    {
        m_sources.emplace_back(source);
    }

    bool Bus::removeSource(const SoundSource *source)
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

    void Bus::release(bool recursive)
    {
        SoundSource::release();

        if (recursive)
        {
            for (auto &source : m_sources)
            {
                if (auto bus = dynamic_cast<Bus *>(source))
                {
                    bus->release(true);
                }
                else
                {
                    source->release();
                }
            }
        }
    }
}
