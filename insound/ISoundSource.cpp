#include "ISoundSource.h"
#include "IEffect.h"
#include "Engine.h"
#include "Command.h"

namespace insound {
    ISoundSource::~ISoundSource()
    {
        for (const auto &effect : m_effects)
        {
            delete effect;
        }
    }

    int ISoundSource::read(const uint8_t **pcmPtr, int length, uint32_t parentClock)
    {
        m_parentClock = parentClock;
        if (m_paused)
            return 0;

        uint8_t *data;
        const auto readBytes = readImpl(&data, length); // populates the buffer

        const auto sampleCount = readBytes / sizeof(float);
        for (auto &effect : m_effects)
        {
            effect->process((float *)data, (int)sampleCount);
        }

        *pcmPtr = data;

        m_clock += length;
        return readBytes;
    }

    void ISoundSource::paused(const bool value)
    {
        m_engine->pushCommand(Command::makeSourcePause(this, value));
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
                m_paused = command.data.source.data.pause.value;
            } break;

            default:
            {
            } break;
        }
    }
}
