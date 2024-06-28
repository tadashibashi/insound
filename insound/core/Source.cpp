#include "Source.h"

#include "Command.h"
#include "CpuIntrinsics.h"
#include "Effect.h"
#include "Engine.h"
#include "Error.h"

#include "effects/PanEffect.h"
#include "effects/VolumeEffect.h"

namespace insound {

#define HANDLE_GUARD() do { if (detail::peekSystemError().code == Result::InvalidHandle) { \
    detail::popSystemError(); \
    INSOUND_PUSH_ERROR(Result::InvalidHandle, __FUNCTION__); \
    return false; \
} } while(0)

    Source::Source() :
        m_engine(),
        m_panner(),
        m_volume(), m_effects(),
        m_outBuffer(), m_inBuffer(),
        m_fadePoints(), m_fadeValue(1.f), m_clock(0),
        m_parentClock(0), m_paused(),
        m_pauseClock(-1), m_unpauseClock(-1), m_releaseOnPauseClock(false),
        m_shouldDiscard(false)
    {

    }

    bool Source::init(Engine *engine, const uint32_t parentClock, const bool paused)
    {
        m_engine = engine;
        m_clock = 0;
        m_parentClock = parentClock;
        m_paused = paused;
        m_pauseClock = -1;
        m_unpauseClock = -1;
        m_shouldDiscard = false;
        m_fadeValue = 1.f;

        m_panner = engine->getObjectPool().allocate<PanEffect>();
        m_volume = engine->getObjectPool().allocate<VolumeEffect>();

        // Immediately add default effects (no need to go through deferred commands)
        applyAddEffect(m_panner.cast<Effect>(), 0);
        applyAddEffect(m_volume.cast<Effect>(), 1);

        // Initialize buffers
        // m_outBuffer.resize(64 * 2 * sizeof(float), 0);
        // m_inBuffer.resize(64 * 2 * sizeof(float), 0);

        return true;
    }

    bool Source::close(bool recursive)
    {
        HANDLE_GUARD();

        return m_engine->releaseSoundRaw(this, recursive);
    }

    bool Source::release()
    {
        HANDLE_GUARD();

        // clean up logic here
        for (auto &effect : m_effects)
        {
            effect->release();
            m_engine->getObjectPool().deallocate(effect);
        }
        m_effects.clear();

        m_shouldDiscard = true;
        return true;
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
        size_t size = points.size();
        for (size_t i = 0; i < size; ++i)
        {
            if (points[i].clock > clock)
            {
                break;
            }

            ++res;
        }

        *outIndex = res;

        return res + 1 < size;
    }

    int Source::read(const uint8_t **pcmPtr, int length)
    {
        if (m_inBuffer.size() < length)
        {
            m_inBuffer.resize(length, 0);
        }

        if (m_outBuffer.size() < length)
        {
            m_outBuffer.resize(length, 0);
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

                int bytesRead = 0;
                // read bytes here
                if (bytesToRead > 0)
                    bytesRead = readImpl(m_outBuffer.data() + i, bytesToRead);

                i += bytesRead;

                if (pauseThisFrame)
                {
                    if (unpauseClock < pauseClock) // if unpause clock comes before pause, unset it, it's redundant
                    {
                        m_unpauseClock = -1;
                        unpauseClock = -1;
                    }

                    m_paused = true;
                    m_pauseClock = -1;

                    if (m_releaseOnPauseClock)
                    {
                        close();
                        break;
                    }
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
            if (effect->process((float *)m_outBuffer.data(), (float *)m_inBuffer.data(), (int)sampleCount))
            {
                std::swap(m_outBuffer, m_inBuffer);

                // clear inBuffer to 0
                std::memset(m_inBuffer.data(), 0, m_inBuffer.size());
            }

        }

        // Apply fade points
        int fadeIndex = -1;
        uint32_t fadeClock = m_parentClock;

        for (auto sample = (float *)m_outBuffer.data(), end = (float *)(m_outBuffer.data() + m_outBuffer.size());
            sample < end;
            )
        {
            if (findFadePointIndex(m_fadePoints, fadeClock, &fadeIndex)) // returns whether we are currently in a fade
            {
                // Get fadepoint data
                const auto clock0 = m_fadePoints[fadeIndex].clock; // starting clock
                const auto value0 = m_fadePoints[fadeIndex].value; // starting value

                const auto clock1 = m_fadePoints[fadeIndex + 1].clock; // end clock
                const auto value1 = m_fadePoints[fadeIndex + 1].value; // end value

                // Perform fade until end or clock1, whichever comes first
                const auto fadeEnd = std::min<uint32_t>(static_cast<uint32_t>(end - sample) / 2U, clock1 + 1 - fadeClock);
                const auto clockDiff = clock1 - clock0;
                const auto valueDiff = value1 - value0;

                uint32_t f = 0;
#if INSOUND_SSE
                const auto clockDiffVec = _mm_set1_ps(static_cast<float>(clockDiff));
                const auto valueDiffVec = _mm_set1_ps(valueDiff);
                const auto value0Vec = _mm_set1_ps(value0);
                for (; f <= fadeEnd - 16; f += 16)
                {
                    const auto clockOffsetVec = _mm_set1_ps(
                        static_cast<float>(fadeClock) - static_cast<float>(clock0));

                    const auto amounts0 = _mm_div_ps(_mm_add_ps(_mm_set_ps(1, 1, 0, 0), clockOffsetVec), clockDiffVec);
                    const auto amounts1 = _mm_div_ps(_mm_add_ps(_mm_set_ps(3, 3, 2, 2), clockOffsetVec), clockDiffVec);
                    const auto amounts2 = _mm_div_ps(_mm_add_ps(_mm_set_ps(5, 5, 4, 4), clockOffsetVec), clockDiffVec);
                    const auto amounts3 = _mm_div_ps(_mm_add_ps(_mm_set_ps(7, 7, 6, 6), clockOffsetVec), clockDiffVec);
                    const auto amounts4 = _mm_div_ps(_mm_add_ps(_mm_set_ps(9, 9, 8, 8), clockOffsetVec), clockDiffVec);
                    const auto amounts5 = _mm_div_ps(_mm_add_ps(_mm_set_ps(11, 11, 10, 10), clockOffsetVec), clockDiffVec);
                    const auto amounts6 = _mm_div_ps(_mm_add_ps(_mm_set_ps(13, 13, 12, 12), clockOffsetVec), clockDiffVec);
                    const auto amounts7 = _mm_div_ps(_mm_add_ps(_mm_set_ps(15, 15, 14, 14), clockOffsetVec), clockDiffVec);
                    const auto result0 = _mm_add_ps(_mm_mul_ps(valueDiffVec, amounts0), value0Vec);
                    const auto result1 = _mm_add_ps(_mm_mul_ps(valueDiffVec, amounts1), value0Vec);
                    const auto result2 = _mm_add_ps(_mm_mul_ps(valueDiffVec, amounts2), value0Vec);
                    const auto result3 = _mm_add_ps(_mm_mul_ps(valueDiffVec, amounts3), value0Vec);
                    const auto result4 = _mm_add_ps(_mm_mul_ps(valueDiffVec, amounts4), value0Vec);
                    const auto result5 = _mm_add_ps(_mm_mul_ps(valueDiffVec, amounts5), value0Vec);
                    const auto result6 = _mm_add_ps(_mm_mul_ps(valueDiffVec, amounts6), value0Vec);
                    const auto result7 = _mm_add_ps(_mm_mul_ps(valueDiffVec, amounts7), value0Vec);
                    _mm_store_ps(sample, _mm_mul_ps(_mm_load_ps(sample), result0));
                    _mm_store_ps(sample + 4, _mm_mul_ps(_mm_load_ps(sample + 4), result1));
                    _mm_store_ps(sample + 8, _mm_mul_ps(_mm_load_ps(sample + 8), result2));
                    _mm_store_ps(sample + 12, _mm_mul_ps(_mm_load_ps(sample + 12), result3));
                    _mm_store_ps(sample + 16, _mm_mul_ps(_mm_load_ps(sample + 16), result4));
                    _mm_store_ps(sample + 20, _mm_mul_ps(_mm_load_ps(sample + 20), result5));
                    _mm_store_ps(sample + 24, _mm_mul_ps(_mm_load_ps(sample + 24), result6));
                    _mm_store_ps(sample + 28, _mm_mul_ps(_mm_load_ps(sample + 28), result7));

                    fadeClock += 16;
                    sample += 32;
                }
#elif INSOUND_WASM_SIMD
                const auto clockDiffVec = wasm_f32x4_splat(static_cast<float>(clockDiff));
                const auto valueDiffVec = wasm_f32x4_splat(valueDiff);
                const auto value0Vec = wasm_f32x4_splat(value0);
                for (; f <= fadeEnd - 16; f += 16)
                {
                    const auto clockOffsetVec = wasm_f32x4_splat(static_cast<float>(fadeClock) - static_cast<float>(clock0));
                    const auto amounts0 = wasm_f32x4_div(wasm_f32x4_add(wasm_f32x4_make(0, 0, 1, 1), clockOffsetVec), clockDiffVec);
                    const auto amounts1 = wasm_f32x4_div(wasm_f32x4_add(wasm_f32x4_make(2, 2, 3, 3), clockOffsetVec), clockDiffVec);
                    const auto amounts2 = wasm_f32x4_div(wasm_f32x4_add(wasm_f32x4_make(4, 4, 5, 5), clockOffsetVec), clockDiffVec);
                    const auto amounts3 = wasm_f32x4_div(wasm_f32x4_add(wasm_f32x4_make(6, 6, 7, 7), clockOffsetVec), clockDiffVec);
                    const auto amounts4 = wasm_f32x4_div(wasm_f32x4_add(wasm_f32x4_make(8, 8, 9, 9), clockOffsetVec), clockDiffVec);
                    const auto amounts5 = wasm_f32x4_div(wasm_f32x4_add(wasm_f32x4_make(10, 10, 11, 11), clockOffsetVec), clockDiffVec);
                    const auto amounts6 = wasm_f32x4_div(wasm_f32x4_add(wasm_f32x4_make(12, 12, 13, 13), clockOffsetVec), clockDiffVec);
                    const auto amounts7 = wasm_f32x4_div(wasm_f32x4_add(wasm_f32x4_make(14, 14, 15, 15), clockOffsetVec), clockDiffVec);

                    const auto results0 = wasm_f32x4_add(wasm_f32x4_mul(valueDiffVec, amounts0), value0Vec);
                    const auto results1 = wasm_f32x4_add(wasm_f32x4_mul(valueDiffVec, amounts1), value0Vec);
                    const auto results2 = wasm_f32x4_add(wasm_f32x4_mul(valueDiffVec, amounts2), value0Vec);
                    const auto results3 = wasm_f32x4_add(wasm_f32x4_mul(valueDiffVec, amounts3), value0Vec);
                    const auto results4 = wasm_f32x4_add(wasm_f32x4_mul(valueDiffVec, amounts4), value0Vec);
                    const auto results5 = wasm_f32x4_add(wasm_f32x4_mul(valueDiffVec, amounts5), value0Vec);
                    const auto results6 = wasm_f32x4_add(wasm_f32x4_mul(valueDiffVec, amounts6), value0Vec);
                    const auto results7 = wasm_f32x4_add(wasm_f32x4_mul(valueDiffVec, amounts7), value0Vec);

                    wasm_v128_store(sample, wasm_f32x4_mul(wasm_v128_load(sample), results0));
                    wasm_v128_store(sample + 4, wasm_f32x4_mul(wasm_v128_load(sample + 4), results1));
                    wasm_v128_store(sample + 8, wasm_f32x4_mul(wasm_v128_load(sample + 8), results2));
                    wasm_v128_store(sample + 12, wasm_f32x4_mul(wasm_v128_load(sample + 12), results3));
                    wasm_v128_store(sample + 16, wasm_f32x4_mul(wasm_v128_load(sample + 16), results4));
                    wasm_v128_store(sample + 20, wasm_f32x4_mul(wasm_v128_load(sample + 20), results5));
                    wasm_v128_store(sample + 24, wasm_f32x4_mul(wasm_v128_load(sample + 24), results6));
                    wasm_v128_store(sample + 28, wasm_f32x4_mul(wasm_v128_load(sample + 28), results7));

                    fadeClock += 16;
                    sample += 32;
                }
#elif INSOUND_ARM_NEON
                const auto clockDiffVec = vdupq_n_f32(static_cast<float>(clockDiff));
                const auto valueDiffVec = vdupq_n_f32(static_cast<float>(valueDiff));
                const auto value0Vec = vdupq_n_f32(static_cast<float>(value0));
                for (; f < fadeEnd - 16; f += 16)
                {
                    const auto clockOffsetVec = vdupq_n_f32(static_cast<float>(fadeClock) - static_cast<float>(clock0));
                    const auto amounts0 = vdivq_f32(vaddq_f32(float32x4_t{0, 0, 1, 1}, clockOffsetVec), clockDiffVec);
                    const auto amounts1 = vdivq_f32(vaddq_f32(float32x4_t{2, 2, 3, 3}, clockOffsetVec), clockDiffVec);
                    const auto amounts2 = vdivq_f32(vaddq_f32(float32x4_t{4, 4, 5, 5}, clockOffsetVec), clockDiffVec);
                    const auto amounts3 = vdivq_f32(vaddq_f32(float32x4_t{6, 6, 7, 7}, clockOffsetVec), clockDiffVec);
                    const auto amounts4 = vdivq_f32(vaddq_f32(float32x4_t{8, 8, 9, 9}, clockOffsetVec), clockDiffVec);
                    const auto amounts5 = vdivq_f32(vaddq_f32(float32x4_t{10, 10, 11, 11}, clockOffsetVec), clockDiffVec);
                    const auto amounts6 = vdivq_f32(vaddq_f32(float32x4_t{12, 12, 13, 13}, clockOffsetVec), clockDiffVec);
                    const auto amounts7 = vdivq_f32(vaddq_f32(float32x4_t{14, 14, 15, 15}, clockOffsetVec), clockDiffVec);

                    const auto results0 = vaddq_f32(vmulq_f32(valueDiffVec, amounts0), value0Vec);
                    const auto results1 = vaddq_f32(vmulq_f32(valueDiffVec, amounts1), value0Vec);
                    const auto results2 = vaddq_f32(vmulq_f32(valueDiffVec, amounts2), value0Vec);
                    const auto results3 = vaddq_f32(vmulq_f32(valueDiffVec, amounts3), value0Vec);
                    const auto results4 = vaddq_f32(vmulq_f32(valueDiffVec, amounts4), value0Vec);
                    const auto results5 = vaddq_f32(vmulq_f32(valueDiffVec, amounts5), value0Vec);
                    const auto results6 = vaddq_f32(vmulq_f32(valueDiffVec, amounts6), value0Vec);
                    const auto results7 = vaddq_f32(vmulq_f32(valueDiffVec, amounts7), value0Vec);

                    vst1q_f32(sample, vmulq_f32(vld1q_f32(sample), results0));
                    vst1q_f32(sample + 4, vmulq_f32(vld1q_f32(sample + 4), results1));
                    vst1q_f32(sample + 8, vmulq_f32(vld1q_f32(sample + 8), results2));
                    vst1q_f32(sample + 12, vmulq_f32(vld1q_f32(sample + 12), results3));
                    vst1q_f32(sample + 16, vmulq_f32(vld1q_f32(sample + 16), results4));
                    vst1q_f32(sample + 20, vmulq_f32(vld1q_f32(sample + 20), results5));
                    vst1q_f32(sample + 24, vmulq_f32(vld1q_f32(sample + 24), results6));
                    vst1q_f32(sample + 28, vmulq_f32(vld1q_f32(sample + 28), results7));

                    sample += 32;
                    fadeClock += 16;
                }
#else
                for (; f <= fadeEnd - 4; f += 4)
                {
                    const auto clockOffset = fadeClock - clock0; // current offset from clock0
                    const float amount0 = (float)(clockOffset) / (float)(clockDiff);
                    const float amount1 = (float)(clockOffset + 1) / (float)(clockDiff);
                    const float amount2 = (float)(clockOffset + 2) / (float)(clockDiff);
                    const float amount3 = (float)(clockOffset + 3) / (float)(clockDiff);
                    const auto result0 = valueDiff * amount0 + value0;
                    const auto result1 = valueDiff * amount1 + value0;
                    const auto result2 = valueDiff * amount2 + value0;
                    const auto result3 = valueDiff * amount3 + value0;
                    sample[0] *= result0;
                    sample[1] *= result0;
                    sample[2] *= result1;
                    sample[3] *= result1;
                    sample[4] *= result2;
                    sample[5] *= result2;
                    sample[6] *= result3;
                    sample[7] *= result3;

                    fadeClock += 4;
                    sample += 8;
                }
#endif
                for (;f < fadeEnd; ++f)
                {
                    const float amount = (float)(fadeClock - clock0) / (float)(clockDiff);
                    const auto result = valueDiff * amount + value0;
                    *sample++ *= result;
                    *sample++ *= result;
                    ++fadeClock;
                }

                m_fadeValue = value1;
            }
            else
            {
                // find end point
                uint32_t endIndex;
                if (fadeIndex + 1 < m_fadePoints.size()) // there's next fadepoint
                {
                    endIndex = std::min<uint32_t>(m_fadePoints[fadeIndex + 1].clock - fadeClock, static_cast<uint32_t>(end - sample));
                }
                else
                {
                    endIndex = static_cast<uint32_t>(end - sample);
                }

                if (m_fadeValue == 1.f) // fade of 1 has no effect, just move the sample ptr
                    sample += endIndex;
                else                    // perform fade multiplier on all samples until endIndex
                {
                    for (uint32_t i = 0; i < endIndex; ++i)
                        *sample++ *= m_fadeValue;
                }
            }

        }

        // Remove all fade points that have been passed
        if (fadeIndex > 0)
            m_fadePoints.erase(m_fadePoints.begin(), m_fadePoints.begin() + (fadeIndex - 1));

        if (pcmPtr)
            *pcmPtr = m_outBuffer.data();

        m_clock += length / (2 * sizeof(float));
        return length;
    }

    Source::Source(Source &&other) noexcept : m_engine(other.m_engine),
        m_panner(other.m_panner), m_volume(other.m_volume), m_effects(std::move(other.m_effects)),
        m_outBuffer(std::move(other.m_outBuffer)), m_inBuffer(std::move(other.m_inBuffer)),
        m_fadePoints(std::move(other.m_fadePoints)), m_fadeValue(other.m_fadeValue),
        m_clock(other.m_clock), m_parentClock(other.m_parentClock),
        m_paused(other.m_paused), m_pauseClock(other.m_pauseClock), m_unpauseClock(other.m_unpauseClock),
        m_releaseOnPauseClock(other.m_releaseOnPauseClock), m_shouldDiscard(other.m_shouldDiscard)
    {}

    bool Source::getPaused(bool *outPaused) const
    {
        HANDLE_GUARD();

        if (outPaused)
        {
            *outPaused = m_paused;
        }
        return true;
    }

    bool Source::pauseAt(const uint32_t clock, const bool shouldStop)
    {
        HANDLE_GUARD();

        return m_engine->pushImmediateCommand(
            Command::makeSourcePause(this, true, shouldStop, clock == UINT32_MAX ? m_parentClock : clock));
    }

    bool Source::unpauseAt(const uint32_t clock)
    {
        HANDLE_GUARD();

        return m_engine->pushImmediateCommand(
            Command::makeSourcePause(this, false, false, clock == UINT32_MAX ? m_parentClock : clock));
    }

    bool Source::setPaused(const bool paused)
    {
        return paused ? pauseAt(UINT32_MAX, false) : unpauseAt(UINT32_MAX);
    }

    Handle<Effect> Source::addEffectImpl(Handle<Effect> effect, int position)
    {
        // No check for validity needed since it was done in `addEffect`
        m_engine->pushCommand(Command::makeSourceEffect(this, true, effect, position));
        return effect;
    }

    bool Source::removeEffect(const Handle<Effect> &effect)
    {
        HANDLE_GUARD();

        m_engine->pushCommand(Command::makeSourceEffect(this, false, effect, -1)); // -1 is discarded in applyCommand

        return true;
    }

    bool Source::getEffect(const int position, Handle<Effect> *outEffect)
    {
        HANDLE_GUARD();

        if (position >= m_effects.size() || position < 0)
        {
            INSOUND_PUSH_ERROR(Result::RangeErr, "Source::getEffect: `position` is out of range");
            return false;
        }

        if (outEffect)
            *outEffect = m_effects[position];

        return true;
    }

    bool Source::getEffect(int position, Handle<const Effect> *outEffect) const
    {
        HANDLE_GUARD();

        if (position >= m_effects.size() || position < 0)
        {
            INSOUND_PUSH_ERROR(Result::RangeErr, "Source::getEffect: `position` is out of range");
            return false;
        }

        if (outEffect)
            *outEffect = (Handle<const Effect>)m_effects[position];

        return true;
    }

    bool Source::getEffectCount(int *outCount) const
    {
        HANDLE_GUARD();

        if (outCount)
        {
            *outCount = (int)m_effects.size();
        }

        return true;
    }

    bool Source::getEngine(Engine **engine)
    {
        HANDLE_GUARD();

        if (engine)
            *engine = m_engine;
        return true;
    }

    bool Source::getEngine(const Engine **engine) const
    {
        HANDLE_GUARD();

        if (engine)
            *engine = m_engine;
        return true;
    }

    bool Source::getClock(uint32_t *outClock) const
    {
        HANDLE_GUARD();

        if (outClock)
        {
            *outClock = m_clock;
        }

        return true;
    }

    bool Source::getParentClock(uint32_t *outClock) const
    {
        HANDLE_GUARD();

        if (outClock)
        {
            *outClock = m_parentClock;
        }

        return true;
    }

    bool Source::getPanner(Handle<PanEffect> *outPanner)
    {
        HANDLE_GUARD();

        if (outPanner)
        {
            *outPanner = m_panner;
        }

        return true;
    }

    bool Source::getPanner(Handle<const PanEffect> *outPanner) const
    {
        HANDLE_GUARD();

        if (outPanner)
            *outPanner = (Handle<const PanEffect>)m_panner;
        return true;
    }

    bool Source::getVolume(float *outVolume) const
    {
        HANDLE_GUARD();

        if (outVolume)
            *outVolume = m_volume->volume();
        return true;
    }

    bool Source::setVolume(const float value)
    {
        HANDLE_GUARD();

        m_volume->volume(value);
        return true;
    }

    bool Source::addFadePoint(const uint32_t clock, const float value)
    {
        HANDLE_GUARD();

        // Push immediately for sample clock accuracy
        m_engine->pushImmediateCommand(Command::makeSourceAddFadePoint(this, clock, value));
        return true;
    }

    bool Source::fadeTo(const float value, const uint32_t length)
    {
        HANDLE_GUARD();

        // Push immediately for sample clock accuracy
        m_engine->pushImmediateCommand(Command::makeSourceAddFadeTo(this, m_parentClock + length, value));
        return true;
    }

    bool Source::removeFadePoints(const uint32_t start, const uint32_t end)
    {
        HANDLE_GUARD();

        // Push immediately for sample clock accuracy
        m_engine->pushImmediateCommand(Command::makeSourceRemoveFadePoint(this, start, end));
        return true;
    }

    bool Source::getFadeValue(float *outValue) const
    {
        HANDLE_GUARD();

        if (outValue)
            *outValue = m_fadeValue;
        return true;
    }

    bool Source::shouldDiscard() const
    {
        return m_shouldDiscard;
    }

    bool Source::swapBuffers(AlignedVector<uint8_t, 16> *buffer)
    {
        HANDLE_GUARD();

        m_outBuffer.swap(*buffer);
        return true;
    }


    // ===== PRIVATE FUNCTIONS ================================================
    // No need to add handle guard here, since checking for validity is the caller's responsibility

    void Source::applyCommand(const SourceCommand &command)
    {
        /// assumes that the command is a source command
        switch (command.type)
        {
            case SourceCommand::AddEffect:
            {
                applyAddEffect(
                    command.effect.effect,
                    command.effect.position);
            } break;

            case SourceCommand::RemoveEffect:
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

            case SourceCommand::SetPause:
            {
                if (command.pause.value)
                {
                    if (command.pause.clock > 0 && command.pause.clock < m_parentClock)
                        m_pauseClock = (int)m_parentClock; // command took too long to send, until the next clock buffer
                    else
                        m_pauseClock = (int)command.pause.clock;

                    m_releaseOnPauseClock = command.pause.releaseOnPause;
                }
                else
                {
                    if (command.pause.clock > 0 && command.pause.clock < m_parentClock)
                        m_unpauseClock = (int)m_parentClock; // command took too long to send, until the next clock buffer
                    else
                        m_unpauseClock = (int)command.pause.clock;
                }
            } break;

            case SourceCommand::AddFadePoint:
            {
                const auto clock = command.addfadepoint.clock;
                const auto value = command.addfadepoint.value;
                applyAddFadePoint(clock, value);
            } break;

            case SourceCommand::AddFadeTo:
            {
                const auto clock = command.addfadepoint.clock;
                const auto value = command.addfadepoint.value;

                // Remove fade points between now and the fade value
                applyRemoveFadePoint(m_parentClock, clock);

                // Set current fade point
                applyAddFadePoint(m_parentClock, m_fadeValue);

                // Set target fade point
                applyAddFadePoint(clock, value);
            } break;

            case SourceCommand::RemoveFadePoint:
            {
                const auto start = command.removefadepoint.begin;
                const auto end = command.removefadepoint.end;
                applyRemoveFadePoint(start, end);
            } break;

            default:
            {
            } break;
        }
    }

    void Source::applyAddFadePoint(const uint32_t clock, const float value)
    {
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
                m_fadePoints.insert(it, FadePoint(clock, value));
                didInsert = true;
                break;
            }
        }

        if (!didInsert)
        {
            m_fadePoints.emplace_back(clock, value);
        }
    }

    void Source::applyRemoveFadePoint(const uint32_t startClock, const uint32_t endClock)
    {
        // remove-erase idiom on all fadepoints that match criteria
        m_fadePoints.erase(std::remove_if(m_fadePoints.begin(), m_fadePoints.end(), [startClock, endClock](const FadePoint &point) {
            return point.clock >= startClock && point.clock < endClock;
        }), m_fadePoints.end());
    }

    void Source::applyAddEffect(const Handle<Effect> &effect, int position)
    {
        const auto it = m_effects.begin() + position;

        effect->m_engine = m_engine; // provide engine to effect

        m_effects.insert(it, effect);
    }

    bool Source::updateParentClock(uint32_t parentClock)
    {
        HANDLE_GUARD();

        m_parentClock = parentClock;
        return true;
    }
}
