#include "PCMSource.h"

#include "Command.h"
#include "CpuIntrinsics.h"
#include "Engine.h"
#include "Error.h"
#include "SoundBuffer.h"

#include <cmath>
#include <iostream>

namespace insound {

/// Check if the handle is valid by checking the system error stack. Return false, if not.
/// (should be thread-safe, since each thread has its own error and system error stacks)
#define HANDLE_GUARD() do { if (detail::peekSystemError().code == Result::InvalidHandle) { \
        detail::popSystemError(); \
        INSOUND_PUSH_ERROR(Result::InvalidHandle, __FUNCTION__); \
        return false; \
    } } while(0)


    PCMSource::PCMSource() : Source(),
        m_buffer(), m_position(0), m_isLooping(),
        m_isOneShot(), m_speed(1.f)
    {
    }

    PCMSource::PCMSource(PCMSource &&other) noexcept : Source(std::move(other)),
        m_buffer(other.m_buffer),
        m_position(other.m_position), m_isLooping(other.m_isLooping), m_isOneShot(other.m_isOneShot),
        m_speed(other.m_speed)
    {

    }

    bool PCMSource::init(Engine *engine, const SoundBuffer *buffer, uint32_t parentClock, bool paused,
                         bool looping, bool oneShot)
    {
        if (!Source::init(engine, parentClock, paused))
            return false;
        m_buffer = buffer;
        m_isLooping = looping;
        m_position = 0;
        m_speed = 1.f;
        m_isOneShot = oneShot;
        return true;
    }

    bool PCMSource::setPosition(const float value)
    {
        HANDLE_GUARD();

        return m_engine->pushImmediateCommand(Command::makePCMSourceSetPosition(this, value));
    }

    bool PCMSource::getSpeed(float *outSpeed) const
    {
        HANDLE_GUARD();

        if (outSpeed)
        {
            *outSpeed = m_speed;
        }

        return true;
    }

    bool PCMSource::setSpeed(float value)
    {
        HANDLE_GUARD();

        return m_engine->pushCommand(Command::makePCMSourceSetSpeed(this, value));
    }

    bool PCMSource::getLooping(bool *outLooping) const
    {
        HANDLE_GUARD();

        if (outLooping)
            *outLooping = m_isLooping;
        return true;
    }

    bool PCMSource::setLooping(bool value)
    {
        HANDLE_GUARD();

        return m_engine->pushCommand(Command::makePCMSourceSetLooping(this, value));
    }

    bool PCMSource::getOneshot(bool *outOneshot) const
    {
        HANDLE_GUARD();

        if (outOneshot)
            *outOneshot = m_isOneShot;
        return true;
    }

    bool PCMSource::setOneshot(bool value)
    {
        HANDLE_GUARD();

        return m_engine->pushCommand(Command::makePCMSourceSetOneShot(this, value));
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

            case PCMSourceCommand::SetLooping:
            {
                m_isLooping = command.setlooping.looping;
            } break;

            case PCMSourceCommand::SetOneShot:
            {
                m_isOneShot = command.setoneshot.oneshot;
            } break;

            default:
            {
                INSOUND_PUSH_ERROR(Result::InvalidArg, "Unknown pcm source command type");
            } break;
        }
    }

    int PCMSource::readImpl(uint8_t *output, int length)
    {
        const auto buffer = (float *)m_buffer->data();
        if (!buffer)
            return 0;

        const auto sampleSize = m_buffer->size() / sizeof(float);
        const auto frameSize = sampleSize / 2;
        const auto sampleLength = length / sizeof(float);
        const auto frameLength = sampleLength / 2;

        if (sampleSize == 0) // prevent zero copy
            return 0;

        if (m_isLooping && m_position >= frameSize) // prevent accidentally reading past buffer, this read probably occurred before the engine could clean the sound up
            return 0;

        const int framesToRead = m_isLooping ? (int)frameLength : std::min<int>((int)frameLength, (int)frameSize - (int)std::ceil(m_position));

        // clear the write buffer
        std::memset(output, 0, length);

        if (framesToRead <= 0)
        {
            return length;
        }


//         if (m_speed != 1.f) // ========= Sample Interpolation ===================================
//         {
// #if INSOUND_SSE
//             const auto oneVec = _mm_set1_ps(1.f);
// #elif INSOUND_WASM_SIMD
//             const auto oneVec = wasm_f32x4_splat(1.f);
// #elif INSOUND_ARM_NEON
//             const auto oneVec = vdupq_n_f32(1.f);
// #endif
//             const auto speed = m_speed;
//             int i = 0;
//             constexpr int UnrolledSize = 8;
//             for (; i <= framesToRead - UnrolledSize; i += UnrolledSize) // visit each frame
//             {
//                 // Collect interpolated sample data and t parameter values
//                 alignas(16) float vals[UnrolledSize * 2], nextVals[UnrolledSize * 2];
//                 alignas(16) float ts[UnrolledSize * 2];
//
//                 // Whether this iteration contains the last frame of the buffer (when `i == UnrolledSize - 1`)
//                 const bool touchesLastFrame = (i + UnrolledSize == framesToRead || (int)m_position + framesToRead + 1 >= frameSize);
//
//                 if (m_isLooping) // (accounts for wrapping around buffer)
//                 {
//                     float curFrame = fmodf((m_position + ((float)(i) * speed)), (float)frameSize);
//                     for (int j = 0; j < UnrolledSize; ++j)
//                     {
//                         const auto baseSample = (int)curFrame * 2 % sampleSize;
//                         const auto j2 = j * 2;
//                         vals[j2] = buffer[baseSample];
//                         vals[j2+1] = buffer[baseSample + 1];
//                         nextVals[j2] = buffer[(baseSample + 2) % sampleSize];
//                         nextVals[j2+1] = buffer[(baseSample + 3) % sampleSize];
//                         ts[j2] = curFrame - floorf(curFrame);
//                         ts[j2 + 1] = ts[j2];
//
//                         curFrame += speed;
//                         if (curFrame > (float)frameSize)
//                             curFrame = fmodf(curFrame, (float)frameSize);
//                     }
//                 }
//                 else // non-looping (less need for bounds checks and modulo operations)
//                 {
//                     float curFrame = m_position + ((float)(i) * speed);
//                     if (curFrame >= frameSize) // sound ended; TODO: solve miscalculation of the remaining frames?
//                     {
//                         i = framesToRead;
//                         break;
//                     }
//
//                     if (touchesLastFrame)
//                     {
//                         for (int j = 0; j < UnrolledSize-1 && curFrame < (float)frameSize - speed; ++j)
//                         {
//                             const auto baseSample = (int)curFrame * 2;
//                             const auto j2 = j * 2;
//                             vals[j2] = buffer[baseSample];
//                             vals[j2+1] = buffer[baseSample + 1];
//                             nextVals[j2] = buffer[baseSample + 2];
//                             nextVals[j2+1] = buffer[baseSample + 3];
//                             ts[j2] = curFrame - floorf(curFrame);
//                             ts[j2 + 1] = ts[j2];
//
//                             curFrame += speed;
//                         }
//
//                         // Manually assign the last buffer frame to our last calculated frame
//                         vals[UnrolledSize * 2 - 2] = buffer[sampleSize - 2];
//                         vals[UnrolledSize * 2 - 1] = buffer[sampleSize - 1];
//                         nextVals[UnrolledSize * 2 - 2] = 0;
//                         nextVals[UnrolledSize * 2 - 1] = 0;
//                         ts[UnrolledSize - 2] = curFrame - floorf(curFrame);
//                         ts[UnrolledSize - 1] = ts[UnrolledSize - 2];
//                     }
//                     else
//                     {
//                         for (int j = 0; j < UnrolledSize && (int)curFrame < frameSize; ++j)
//                         {
//                             const auto baseSample = (int)curFrame * 2;
//                             const auto j2 = j * 2;
//                             vals[j2] = buffer[baseSample];
//                             vals[j2+1] = buffer[baseSample + 1];
//                             nextVals[j2] = buffer[baseSample + 2];
//                             nextVals[j2+1] = buffer[baseSample + 3];
//                             ts[j2] = curFrame - floorf(curFrame);
//                             ts[j2+ 1] = ts[j2];
//
//                             curFrame += speed;
//                         }
//                     }
//                 }
//
// #if INSOUND_SSE
//                 const auto tVec0 = _mm_load_ps(ts);
//                 const auto tVec1 = _mm_load_ps(ts + 4);
//                 const auto tVec2 = _mm_load_ps(ts + 8);
//                 const auto tVec3 = _mm_load_ps(ts + 12);
//                 const auto omtVec0 = _mm_sub_ps(oneVec, tVec0);
//                 const auto omtVec1 = _mm_sub_ps(oneVec, tVec1);
//                 const auto omtVec2 = _mm_sub_ps(oneVec, tVec2);
//                 const auto omtVec3 = _mm_sub_ps(oneVec, tVec3);
//
//                 const auto bufferVec0 = _mm_load_ps(vals);
//                 const auto bufferVec1 = _mm_load_ps(vals + 4);
//                 const auto bufferVec2 = _mm_load_ps(vals + 8);
//                 const auto bufferVec3 = _mm_load_ps(vals + 12);
//                 const auto bufferNextVec0 = _mm_load_ps(nextVals);
//                 const auto bufferNextVec1 = _mm_load_ps(nextVals + 4);
//                 const auto bufferNextVec2 = _mm_load_ps(nextVals + 8);
//                 const auto bufferNextVec3 = _mm_load_ps(nextVals + 12);
//
//                 const auto result0 = _mm_add_ps(_mm_mul_ps(bufferVec0, omtVec0), _mm_mul_ps(bufferNextVec0, tVec0));
//                 const auto result1 = _mm_add_ps(_mm_mul_ps(bufferVec1, omtVec1), _mm_mul_ps(bufferNextVec1, tVec1));
//                 const auto result2 = _mm_add_ps(_mm_mul_ps(bufferVec2, omtVec2), _mm_mul_ps(bufferNextVec2, tVec2));
//                 const auto result3 = _mm_add_ps(_mm_mul_ps(bufferVec3, omtVec3), _mm_mul_ps(bufferNextVec3, tVec3));
//
//                 const auto i2 = i * 2;
//                 _mm_store_ps((float *)output + i2, result0);
//                 _mm_store_ps((float *)output + i2 + 4, result1);
//                 _mm_store_ps((float *)output + i2 + 8, result2);
//                 _mm_store_ps((float *)output + i2 + 12, result3);
// #elif INSOUND_WASM_SIMD
//                 const auto tVec0 = wasm_v128_load(ts);
//                 const auto tVec1 = wasm_v128_load(ts + 4);
//                 const auto tVec2 = wasm_v128_load(ts + 8);
//                 const auto tVec3 = wasm_v128_load(ts + 12);
//                 const auto omtVec0 = wasm_f32x4_sub(oneVec, tVec0);
//                 const auto omtVec1 = wasm_f32x4_sub(oneVec, tVec1);
//                 const auto omtVec2 = wasm_f32x4_sub(oneVec, tVec2);
//                 const auto omtVec3 = wasm_f32x4_sub(oneVec, tVec3);
//
//                 const auto bufferVec0 = wasm_v128_load(vals);
//                 const auto bufferVec1 = wasm_v128_load(vals + 4);
//                 const auto bufferVec2 = wasm_v128_load(vals + 8);
//                 const auto bufferVec3 = wasm_v128_load(vals + 12);
//                 const auto bufferNextVec0 = wasm_v128_load(nextVals);
//                 const auto bufferNextVec1 = wasm_v128_load(nextVals + 4);
//                 const auto bufferNextVec2 = wasm_v128_load(nextVals + 8);
//                 const auto bufferNextVec3 = wasm_v128_load(nextVals + 12);
//
//                 const auto result0 = wasm_f32x4_add(wasm_f32x4_mul(bufferVec0, omtVec0), wasm_f32x4_mul(bufferNextVec0, tVec0));
//                 const auto result1 = wasm_f32x4_add(wasm_f32x4_mul(bufferVec1, omtVec1), wasm_f32x4_mul(bufferNextVec1, tVec1));
//                 const auto result2 = wasm_f32x4_add(wasm_f32x4_mul(bufferVec2, omtVec2), wasm_f32x4_mul(bufferNextVec2, tVec2));
//                 const auto result3 = wasm_f32x4_add(wasm_f32x4_mul(bufferVec3, omtVec3), wasm_f32x4_mul(bufferNextVec3, tVec3));
//
//                 const auto i2 = i * 2;
//                 wasm_v128_store((float *)output + i2, result0);
//                 wasm_v128_store((float *)output + i2 + 4, result1);
//                 wasm_v128_store((float *)output + i2 + 8, result2);
//                 wasm_v128_store((float *)output + i2 + 12, result3);
// #elif INSOUND_ARM_NEON
//                 const auto tVec0 = vld1q_f32(ts);
//                 const auto tVec1 = vld1q_f32(ts + 4);
//                 const auto tVec2 = vld1q_f32(ts + 8);
//                 const auto tVec3 = vld1q_f32(ts + 12);
//                 const auto omtVec0 = vsubq_f32(oneVec, tVec0);
//                 const auto omtVec1 = vsubq_f32(oneVec, tVec1);
//                 const auto omtVec2 = vsubq_f32(oneVec, tVec2);
//                 const auto omtVec3 = vsubq_f32(oneVec, tVec3);
//
//                 const auto bufferVec0 = vld1q_f32(vals);
//                 const auto bufferVec1 = vld1q_f32(vals + 4);
//                 const auto bufferVec2 = vld1q_f32(vals + 8);
//                 const auto bufferVec3 = vld1q_f32(vals + 12);
//                 const auto bufferNextVec0 = vld1q_f32(nextVals);
//                 const auto bufferNextVec1 = vld1q_f32(nextVals + 4);
//                 const auto bufferNextVec2 = vld1q_f32(nextVals + 8);
//                 const auto bufferNextVec3 = vld1q_f32(nextVals + 12);
//
//                 const auto result0 = vmlaq_f32(vmulq_f32(bufferVec0, omtVec0), bufferNextVec0, tVec0);
//                 const auto result1 = vmlaq_f32(vmulq_f32(bufferVec1, omtVec1), bufferNextVec1, tVec1);
//                 const auto result2 = vmlaq_f32(vmulq_f32(bufferVec2, omtVec2), bufferNextVec2, tVec2);
//                 const auto result3 = vmlaq_f32(vmulq_f32(bufferVec3, omtVec3), bufferNextVec3, tVec3);
//
//                 const auto i2 = i * 2;
//                 vst1q_f32((float *)output + i2, result0);
//                 vst1q_f32((float *)output + i2 + 4, result1);
//                 vst1q_f32((float *)output + i2 + 8, result2);
//                 vst1q_f32((float *)output + i2 + 12, result3);
//     #else
//                 const auto i2 = i * 2;
//                 ((float *)output)[i2] =     vals[0]  * (1.f-ts[0])  + nextVals[0]  * ts[0];
//                 ((float *)output)[i2 + 1] = vals[1]  * (1.f-ts[1])  + nextVals[1]  * ts[1];
//                 ((float *)output)[i2 + 2] = vals[2]  * (1.f-ts[2])  + nextVals[2]  * ts[2];
//                 ((float *)output)[i2 + 3] = vals[3]  * (1.f-ts[3])  + nextVals[3]  * ts[3];
//                 ((float *)output)[i2 + 4] = vals[4]  * (1.f-ts[4])  + nextVals[4]  * ts[4];
//                 ((float *)output)[i2 + 5] = vals[5]  * (1.f-ts[5])  + nextVals[5]  * ts[5];
//                 ((float *)output)[i2 + 6] = vals[6]  * (1.f-ts[6])  + nextVals[6]  * ts[6];
//                 ((float *)output)[i2 + 7] = vals[7]  * (1.f-ts[7])  + nextVals[7]  * ts[7];
//                 ((float *)output)[i2 + 8] = vals[8]  * (1.f-ts[8])  + nextVals[8]  * ts[8];
//                 ((float *)output)[i2 + 9] = vals[9]  * (1.f-ts[9])  + nextVals[9]  * ts[9];
//                 ((float *)output)[i2 +10] = vals[10] * (1.f-ts[10]) + nextVals[10] * ts[10];
//                 ((float *)output)[i2 +11] = vals[11] * (1.f-ts[11]) + nextVals[11] * ts[11];
//                 ((float *)output)[i2 +12] = vals[12] * (1.f-ts[12]) + nextVals[12] * ts[12];
//                 ((float *)output)[i2 +13] = vals[13] * (1.f-ts[13]) + nextVals[13] * ts[13];
//                 ((float *)output)[i2 +14] = vals[14] * (1.f-ts[14]) + nextVals[14] * ts[14];
//                 ((float *)output)[i2 +15] = vals[15] * (1.f-ts[15]) + nextVals[15] * ts[15];
//     #endif
//
//             }
//
//             for (; i < framesToRead; ++i) // catch leftovers
//             {
//                 // current sample position
//                 const float framef = fmodf(((float)m_position + ((float)i * m_speed)), (float)frameSize);
//                 const float t = framef - floorf(framef);
//
//                 const int indexL = (int)framef * 2; // left sample index
//                 const int nextIndexL = (indexL + 2) % (int)sampleSize;
//                 const int indexR = ((int)framef * 2 + 1); // right sample index
//                 const int nextIndexR = (indexR + 2) % (int)sampleSize;
//
//                 const auto tMinus1 = 1.f-t;
//
//                 ((float *)output)[i * 2] = buffer[indexL] * (tMinus1) + buffer[nextIndexL] * (t);
//                 ((float *)output)[i * 2 + 1] = buffer[indexR] * (tMinus1) + buffer[nextIndexR] * (t);
//             }
//         }
//         else // ====== No interpolation =======================================================
        {
            // We just need to copy the bytes directly to the out buffer, accounting for loop mechanic

            const auto bufferSize  = static_cast<int64_t>(m_buffer->size());
            const auto baseBytePos = static_cast<int64_t>(m_position) * 2LL * static_cast<int64_t>(sizeof(float));

            int64_t bytesRead = 0;
            while(bytesRead < length) {
                auto bufferBytePos = (baseBytePos + bytesRead) % bufferSize;
                auto bytesToRead = std::min(bufferSize - bufferBytePos, static_cast<int64_t>(length) - bytesRead);

                // Copy from here until end of requested length, or the end of the buffer
                std::memcpy(output + bytesRead, m_buffer->data() + bufferBytePos, bytesToRead);

                bytesRead += bytesToRead;

                if (!m_isLooping)
                    break;
            }
        }

        // Update buffer position head
        if (m_isLooping)
        {
            m_position = fmodf(m_position + (float)framesToRead * m_speed, (float)frameSize);
        }
        else
        {
            m_position += (float)framesToRead * m_speed;

            // Release sound if it ended and is a oneshot
            if (m_isOneShot && m_position >= (float)frameSize)
            {
                close();
            }
        }

        // Report the number of bytes read
        return (int)framesToRead * (int)sizeof(float) * 2;
    }

    bool PCMSource::getEnded(bool *outEnded) const
    {
        HANDLE_GUARD();

        if (outEnded)
            *outEnded = m_position >= (float)m_buffer->size() / (sizeof(float) * 2.f);
        return true;
    }

    bool PCMSource::getPosition(float *outPosition) const
    {
        HANDLE_GUARD();

        if (outPosition)
            *outPosition = m_position;

        return true;
    }
}
