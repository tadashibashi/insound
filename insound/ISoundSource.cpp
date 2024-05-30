#include "ISoundSource.h"

#include <iostream>

#include "IEffect.h"
#include "Engine.h"
#include "Command.h"
#include "effects/PanEffect.h"
#include "effects/VolumeEffect.h"

namespace insound {
    ISoundSource::ISoundSource(Engine *engine, uint32_t parentClock, bool paused): m_paused(paused),
        m_effects(), m_engine(engine), m_clock(0), m_parentClock(parentClock),
        m_pauseClock(-1), m_unpauseClock(-1), m_panner(nullptr), m_volume(nullptr)
    {
        m_panner = (PanEffect *)insertEffect(new PanEffect(engine), 0);
        m_volume = (VolumeEffect *)insertEffect(new VolumeEffect(engine), 1);
    }

    ISoundSource::~ISoundSource()
    {
        for (const auto &effect : m_effects)
        {
            delete effect;
        }
    }

    int ISoundSource::read(const uint8_t **pcmPtr, int length)
    {
        if (m_inBuffer.size() < length)
        {
            m_inBuffer.resize(length);
        }

        if (m_outBuffer.size() < length)
        {
            m_outBuffer.resize(length);
        }

        std::memset(m_outBuffer.data(), 0, m_outBuffer.size());

        int64_t unpauseClock = (int64_t)m_unpauseClock - (int64_t)m_parentClock;
        int64_t pauseClock = (int64_t)m_pauseClock - (int64_t)m_parentClock;

        for (int i = 0; i < length;)
        {
            if (m_paused)
            {
                // next unpause occurs within this chunk
                if (unpauseClock < (length - i) / (2 * sizeof(float)) && unpauseClock > -1)
                {
                    i += (int)unpauseClock;

                    // if (pauseClock < unpauseClock) // if pause clock comes before unpause, unset it, it's redundant
                    // {
                    //     m_pauseClock = -1;
                    //     pauseClock = -1;
                    // }

                    if (pauseClock > -1)
                        pauseClock -= unpauseClock;
                    std::cout << "Unpause occured at clock: " << m_parentClock + unpauseClock << '\n';
                    m_unpauseClock = -1;
                    m_paused = false;
                }
                else
                {
                    break;
                }
            }
            else
            {
                // check if there is an pause clock ahead to see how many samples to read until then
                bool pauseThisFrame = (pauseClock < (length - i) / (2 * sizeof(float)) && pauseClock > -1);
                int bytesToRead = pauseThisFrame ? pauseClock : length - i;

                // read bytes here
                readImpl(m_outBuffer.data() + i, bytesToRead);

                i += bytesToRead;

                if (pauseThisFrame)
                {
                    // if (unpauseClock < pauseClock)
                    // {
                    //     m_unpauseClock = -1;
                    //     unpauseClock = -1;
                    // }
                    std::cout << "Pause occured at clock: " << m_parentClock + pauseClock << '\n';
                    m_paused = true;
                    m_pauseClock = -1;
                }

                if (pauseClock > -1)
                    pauseClock -= bytesToRead / (2 * sizeof(float)); // 2 for each channel
                if (unpauseClock > -1)
                    unpauseClock -= bytesToRead / (2 * sizeof(float)); // 2 for each channel
            }
        }

        const auto sampleCount = length / sizeof(float);
        for (auto &effect : m_effects)
        {
            // clear inBuffer
            std::memset(m_inBuffer.data(), 0, m_inBuffer.size());

            effect->process((float *)m_outBuffer.data(), (float *)m_inBuffer.data(), (int)sampleCount);
            std::swap(m_outBuffer, m_inBuffer);
        }

        *pcmPtr = m_outBuffer.data();

        m_clock += length / (2 * sizeof(float));
        return length;
    }

    void ISoundSource::paused(const bool value, const uint32_t clock)
    {
        m_engine->pushImmediateCommand(Command::makeSourcePause(this, value, clock == UINT32_MAX ? m_parentClock : clock));
    }

    IEffect * ISoundSource::insertEffect(IEffect *effect, int position)
    {
        m_engine->pushCommand(Command::makeSourceEffect(this, true, effect, position));
        return effect;
    }

    void ISoundSource::removeEffect(IEffect *effect)
    {
        m_engine->pushCommand(Command::makeSourceEffect(this, false, effect, -1)); // -1 is discarded in applyCommand
    }

    void ISoundSource::applyCommand(const Command &command)
    {
        /// assumes that the command is a source command
        switch (command.data.source.type)
        {
            case SourceCommandType::AddEffect:
            {
                m_effects.insert(m_effects.begin() + command.data.source.data.effect.position,
                    command.data.source.data.effect.effect);
            } break;

            case SourceCommandType::RemoveEffect:
            {
                const auto effect = command.data.source.data.effect.effect;
                for (auto it = m_effects.begin(); it != m_effects.end(); ++it)
                {
                    if (*it == effect)
                    {
                        m_effects.erase(it);
                        break;
                    }
                }
            } break;

            case SourceCommandType::SetPause:
            {
                if (command.data.source.data.pause.value)
                {
                    m_pauseClock = command.data.source.data.pause.clock;
                }
                else
                {
                    m_unpauseClock = command.data.source.data.pause.clock;
                }
            } break;

            case SourceCommandType::AddFadePoint:
            {
                const auto clock = command.data.source.data.addfadepoint.clock;
                const auto value = command.data.source.data.addfadepoint.value;

                bool didInsert = false;
                for (auto it = m_fadePoints.begin(); it != m_fadePoints.end(); ++it)
                {
                    if (clock == it->clock) // replace the value
                    {
                        it->value = value;
                        didInsert = true;
                        break;
                    }

                    if (clock < it->clock)
                    {
                        m_fadePoints.insert(it, FadePoint{.clock=clock, .value=value});
                        didInsert = true;
                        break;
                    }
                }

                if (!didInsert)
                {
                    m_fadePoints.emplace_back(FadePoint{.clock=clock, .value=value});
                }
            } break;

            case SourceCommandType::RemoveFadePoint:
            {
                const auto start = command.data.source.data.removefadepoint.begin;
                const auto end = command.data.source.data.removefadepoint.end;

                // remove-erase idiom on all fadepoints that match criteria
                m_fadePoints.erase(std::remove_if(m_fadePoints.begin(), m_fadePoints.end(), [start, end](const FadePoint &point) {
                    return point.clock >=  start && point.clock < end;
                }), m_fadePoints.end());

            } break;

            default:
            {
            } break;
        }
    }

    float ISoundSource::volume() const
    {
        return m_volume->volume();
    }

    void ISoundSource::volume(const float value)
    {
        m_volume->volume(value);
    }

    void ISoundSource::addFadePoint(const uint32_t clock, const float value)
    {
        // pushed immediately due to need for immediate sample clock accuracy
        m_engine->pushImmediateCommand(Command::makeSourceAddFadePoint(this, clock, value));
    }

    void ISoundSource::removeFadePoints(const uint32_t start, const uint32_t end)
    {
        // pushed immeidately due to need for immediate sample clock accuracy
        m_engine->pushImmediateCommand(Command::makeSourceRemoveFadePoint(this, start, end));
    }
}
