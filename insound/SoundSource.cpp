#include "SoundSource.h"

#include "Effect.h"
#include "Engine.h"
#include "Command.h"
#include "Error.h"

#include "effects/PanEffect.h"
#include "effects/VolumeEffect.h"

namespace insound {
    SoundSource::SoundSource(Engine *engine, const uint32_t parentClock, const bool paused) :
        m_engine(engine),
        m_effects(),
        m_outBuffer(), m_inBuffer(),
        m_clock(0), m_parentClock(parentClock),
        m_paused(paused), m_pauseClock(-1), m_unpauseClock(-1),
        m_fadePoints(), m_fadeValue(1.f),
        m_panner(new PanEffect()), m_volume(new VolumeEffect()),
        m_shouldDiscard(false)
    {
        // Immediately add default effects (no need to go through deferred commands)
        applyAddEffect(m_panner, 0);
        applyAddEffect(m_volume, 1);

        // Initialize buffers
        uint32_t bufferSize;
        if (engine->getBufferSize(&bufferSize))
        {
            m_outBuffer.resize(bufferSize, 0);
            m_inBuffer.resize(bufferSize, 0);
        }
    }

    SoundSource::~SoundSource()
    {
        for (const auto &effect : m_effects)
        {
            delete effect;
        }
    }

    /// Find starting fade point index, if there is no fade, e.g. < 2 points available, or the last fadepoint clock time
    /// was surpassed, -1 is returned.
    /// @param points list of points to check, must be sorted by clock time, low to high
    /// @param clock  current clock point to check
    /// @param outIndex [out] index to retrieve - this value is the last fadepoint that is less than `clock`, thus the
    ///                       first of two points to interpolate the resulting fade value.
    /// @returns whether there is a next available fade at the index after `outIndex`. Since there must be a second
    ///          fade point to interpolate between, this also means whether to perform the fade or not.
    static bool findFadePointIndex(const std::vector<FadePoint> &points, const uint32_t clock, int *outIndex)
    {
        int res = -1;
        for (const auto &point : points)
        {
            if (point.clock > clock)
            {
                break;
            }

            ++res;
        }

        if (outIndex)
            *outIndex = res;

        return res + 1 < points.size();
    }

    int SoundSource::read(const uint8_t **pcmPtr, int length)
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
                // Next unpause occurs within this chunk
                if (unpauseClock < (length - i) / (2 * sizeof(float)) && unpauseClock > -1)
                {
                    i += (int)unpauseClock;

                    if (pauseClock < unpauseClock) // if pause clock comes before unpause, unset it, it's redundant
                    {
                        m_pauseClock = -1;
                        pauseClock = -1;
                    }

                    if (pauseClock > -1)
                        pauseClock -= unpauseClock;
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
                // Check if there is a pause clock ahead to see how many samples to read until then
                const bool pauseThisFrame = (pauseClock < (length - i) / (2 * sizeof(float)) && pauseClock > -1);
                const int bytesToRead = pauseThisFrame ? (int)pauseClock : length - i;

                // read bytes here
                readImpl(m_outBuffer.data() + i, bytesToRead);

                i += bytesToRead;

                if (pauseThisFrame)
                {
                    if (unpauseClock < pauseClock) // if unpause clock comes before pause, unset it, it's redundant
                    {
                        m_unpauseClock = -1;
                        unpauseClock = -1;
                    }

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
        for (const auto &effect : m_effects)
        {
            // clear inBuffer to 0
            std::memset(m_inBuffer.data(), 0, m_inBuffer.size());

            effect->process((float *)m_outBuffer.data(), (float *)m_inBuffer.data(), (int)sampleCount);
            std::swap(m_outBuffer, m_inBuffer);
        }

        // Apply fade points
        int fadeIndex = -1;
        uint32_t fadeClock = m_parentClock;

        for (auto sample = (float *)m_outBuffer.data(), end = (float *)(m_outBuffer.data() + m_outBuffer.size());
            sample != end;
            ++sample)
        {
            if (findFadePointIndex(m_fadePoints, fadeClock, &fadeIndex)) // calculate fade value
            {
                const auto clock0 = m_fadePoints[fadeIndex].clock;
                const auto clock1 = m_fadePoints[fadeIndex + 1].clock;
                const float amount = (float)(fadeClock - clock0) / (float)(clock1 - clock0);

                const auto value0 = m_fadePoints[fadeIndex].value;
                const auto value1 = m_fadePoints[fadeIndex + 1].value;

                m_fadeValue =  (value1 - value0) * amount + value0;
            }

            // apply fade on sample
            *sample *= m_fadeValue;

            ++fadeClock;
        }

        // Remove all fade points that have been passed
        if (fadeIndex > 0)
            m_fadePoints.erase(m_fadePoints.begin(), m_fadePoints.begin() + (fadeIndex - 1));

        if (pcmPtr)
            *pcmPtr = m_outBuffer.data();

        m_clock += length / (2 * sizeof(float));
        return length;
    }

    bool SoundSource::getPaused(bool *outPaused) const
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::getPaused");
            return false;
        }

        if (outPaused)
        {
            *outPaused = m_paused;
        }
        return true;
    }

    bool SoundSource::setPaused(const bool value, const uint32_t clock)
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::setPaused");
            return false;
        }

        m_engine->pushImmediateCommand(Command::makeSourcePause(this, value, clock == UINT32_MAX ? m_parentClock : clock));
        return true;
    }

    Effect *SoundSource::addEffectImpl(Effect *effect, int position)
    {
        // No check for validity needed since it was done in `addEffect`
        m_engine->pushCommand(Command::makeSourceEffect(this, true, effect, position));
        return effect;
    }

    bool SoundSource::removeEffect(Effect *effect)
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::removeEffect");
            return false;
        }

        m_engine->pushCommand(Command::makeSourceEffect(this, false, effect, -1)); // -1 is discarded in applyCommand

        return true;
    }

    bool SoundSource::getEffect(const int position, Effect **outEffect) const
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::getEffect");
            return false;
        }

        if (outEffect) // todo: add bounds checking?
            *outEffect = m_effects[position];

        return true;
    }

    bool SoundSource::getEffectCount(int *outCount)
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::getEffectCount");
            return false;
        }

        if (outCount)
        {
            *outCount = (int)m_effects.size();
        }

        return true;
    }

    bool SoundSource::getClock(uint32_t *outClock) const
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::getClock");
            return false;
        }

        if (outClock)
        {
            *outClock = m_clock;
        }

        return true;
    }

    bool SoundSource::getParentClock(uint32_t *outClock) const
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::getParentClock");
            return false;
        }

        if (outClock)
        {
            *outClock = m_parentClock;
        }

        return true;
    }

    bool SoundSource::updateParentClock(uint32_t parentClock)
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::updateParentClock");
            return false;
        }

        m_parentClock = parentClock;
        return true;
    }

    void SoundSource::applyCommand(const SoundSourceCommand &command)
    {
        /// assumes that the command is a source command
        switch (command.type)
        {
            case SoundSourceCommand::AddEffect:
            {
                applyAddEffect(
                    command.effect.effect,
                    command.effect.position);
            } break;

            case SoundSourceCommand::RemoveEffect:
            {
                const auto effect = command.effect.effect;
                for (auto it = m_effects.begin(); it != m_effects.end(); ++it)
                {
                    if (*it == effect)
                    {
                        m_effects.erase(it);
                        break;
                    }
                }
            } break;

            case SoundSourceCommand::SetPause:
            {
                if (command.pause.value)
                {
                    m_pauseClock = (int)command.pause.clock;
                }
                else
                {
                    m_unpauseClock = (int)command.pause.clock;
                }
            } break;

            case SoundSourceCommand::AddFadePoint:
            {
                const auto clock = command.addfadepoint.clock;
                const auto value = command.addfadepoint.value;

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

            case SoundSourceCommand::RemoveFadePoint:
            {
                const auto start = command.removefadepoint.begin;
                const auto end = command.removefadepoint.end;

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

    void SoundSource::applyAddEffect(Effect *effect, int position)
    {
        const auto it = m_effects.begin() + position;

        effect->m_engine = m_engine; // provide engine

        m_effects.insert(it, effect);
    }

    bool SoundSource::getVolume(float *outVolume) const
    {
        if (detail::popSystemError().code == Result::InvalidHandle)
        {
            pushError(Result::InvalidHandle, "SoundSource::getVolume");
            return false;
        }

        *outVolume = m_volume->volume();
        return true;
    }

    bool SoundSource::setVolume(const float value)
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::setVolume");
            return false;
        }

        m_volume->volume(value);
        return true;
    }

    bool SoundSource::addFadePoint(const uint32_t clock, const float value)
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::addFadePoint");
            return false;
        }

        // pushed immediately due to need for immediate sample clock accuracy
        m_engine->pushImmediateCommand(Command::makeSourceAddFadePoint(this, clock, value));
        return true;
    }

    bool SoundSource::removeFadePoints(const uint32_t start, const uint32_t end)
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::removeFadePoints");
            return false;
        }

        // pushed immeidately due to need for immediate sample clock accuracy
        m_engine->pushImmediateCommand(Command::makeSourceRemoveFadePoint(this, start, end));
        return true;
    }

    bool SoundSource::getFadeValue(float *outValue) const
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::getFadeValue");
            return false;
        }

        if (outValue)
            *outValue = m_fadeValue;
        return true;
    }

    bool SoundSource::shouldDiscard(bool *outShouldDiscard) const
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::shouldDiscard");
            return false;
        }


        if (outShouldDiscard)
        {
            *outShouldDiscard = m_shouldDiscard;
        }

        return true;
    }

    bool SoundSource::swapBuffers(std::vector<uint8_t> *buffer)
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::swapBuffers");
            return false;
        }

        m_outBuffer.swap(*buffer);
        return true;
    }

    bool SoundSource::release()
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "SoundSource::release");
            return false;
        }


        m_engine->flagDiscard(this);
        return true;
    }
}
