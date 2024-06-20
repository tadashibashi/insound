#include "PanEffect.h"

#include "../CpuIntrinsics.h"

#include "../Error.h"
#include "../util.h"

namespace insound {
    bool PanEffect::process(const float *input, float *output, const int count)
    {
        if (m_left == 1.f && m_right == 1.f)
            return false;

        const auto left = m_left, right = m_right;
        int i = 0;
#if INSOUND_SSE
        auto a =  _mm_set_ps(1.f - left, 1.f - right, 1.f - left, 1.f - right); // _mm_set_ps requires reverse order
        auto b = _mm_set_ps(right, left, right, left);
        for(; i <= count - 16; i += 16)
        {
            auto inputVecA0 = _mm_set_ps(input[i+2], input[i+3], input[i],  input[i + 1]);
            auto inputVecA1 = _mm_set_ps(input[i + 2 + 4], input[i + 3 + 4], input[i + 4],  input[i + 1 + 4]);
            auto inputVecA2 = _mm_set_ps(input[i + 2 + 8], input[i + 3 + 8], input[i + 8],  input[i + 1 + 8]);
            auto inputVecA3 = _mm_set_ps(input[i + 2 + 12], input[i + 3 + 12], input[i + 12], input[i + 1 + 12]);
            auto inputVecB0 = _mm_load_ps(input + i);
            auto inputVecB1 = _mm_load_ps(input + i + 4);
            auto inputVecB2 = _mm_load_ps(input + i + 8);
            auto inputVecB3 = _mm_load_ps(input + i + 12);
            auto result0 = _mm_add_ps(_mm_mul_ps(inputVecA0, a), _mm_mul_ps(inputVecB0, b));
            auto result1 = _mm_add_ps(_mm_mul_ps(inputVecA1, a), _mm_mul_ps(inputVecB1, b));
            auto result2 = _mm_add_ps(_mm_mul_ps(inputVecA2, a), _mm_mul_ps(inputVecB2, b));
            auto result3 = _mm_add_ps(_mm_mul_ps(inputVecA3, a), _mm_mul_ps(inputVecB3, b));
            _mm_store_ps(output + i, result0);
            _mm_store_ps(output + i + 4, result1);
            _mm_store_ps(output + i + 8, result2);
            _mm_store_ps(output + i + 12, result3);
        }
#elif INSOUND_WASM_SIMD
        auto a = wasm_f32x4_make(1.f - right, 1.f - left, 1.f - right, 1.f - left);
        auto b = wasm_f32x4_make(left, right, left, right);
        for (; i <= count - 16; i += 16)
        {
            auto inputVecA0 = wasm_f32x4_make(input[i + 1], input[i], input[i + 3], input[i + 2]);
            auto inputVecA1 = wasm_f32x4_make(input[i + 1 + 4], input[i + 4], input[i + 3 + 4], input[i + 2 + 4]);
            auto inputVecA2 = wasm_f32x4_make(input[i + 1 + 8], input[i + 8], input[i + 3 + 8], input[i + 2 + 8]);
            auto inputVecA3 = wasm_f32x4_make(input[i + 1 + 12], input[i + 12], input[i + 3 + 12], input[i + 2 + 12]);
            auto inputVecB0 = wasm_v128_load(input + i);
            auto inputVecB1 = wasm_v128_load(input + i + 4);
            auto inputVecB2 = wasm_v128_load(input + i + 8);
            auto inputVecB3 = wasm_v128_load(input + i + 12);
            auto result0 = wasm_f32x4_add(wasm_f32x4_mul(inputVecA0, a), wasm_f32x4_mul(inputVecB0, b));
            auto result1 = wasm_f32x4_add(wasm_f32x4_mul(inputVecA1, a), wasm_f32x4_mul(inputVecB1, b));
            auto result2 = wasm_f32x4_add(wasm_f32x4_mul(inputVecA2, a), wasm_f32x4_mul(inputVecB2, b));
            auto result3 = wasm_f32x4_add(wasm_f32x4_mul(inputVecA3, a), wasm_f32x4_mul(inputVecB3, b));
            wasm_v128_store(output + i, result0);
            wasm_v128_store(output + i + 4, result1);
            wasm_v128_store(output + i + 8, result2);
            wasm_v128_store(output + i + 12, result3);
        }
#elif INSOUND_ARM_NEON
        float32x4_t a {1.f - m_right, 1.f - m_left, 1.f - m_right, 1.f - m_left};
        float32x4_t b { m_left, m_right, m_left, m_right };
        for (; i <= count - 16; i += 16)
        {
            const auto inputVecA0 = float32x4_t{input[i + 1], input[i], input[i + 3], input[i + 2]};
            const auto inputVecA1 = float32x4_t{input[i + 1 + 4], input[i + 4], input[i + 3 + 4], input[i + 2 + 4]};
            const auto inputVecA2 = float32x4_t{input[i + 1 + 8], input[i + 8], input[i + 3 + 8], input[i + 2 + 8]};
            const auto inputVecA3 = float32x4_t{input[i + 1 + 12], input[i + 12], input[i + 3 + 12], input[i + 2 + 12]};

            const auto inputVecB0 = vld1q_f32(input + i);
            const auto inputVecB1 = vld1q_f32(input + i + 4);
            const auto inputVecB2 = vld1q_f32(input + i + 8);
            const auto inputVecB3 = vld1q_f32(input + i + 12);

            const auto result0 = vmlaq_f32(vmulq_f32(inputVecA0, a), inputVecB0, b);
            const auto result1 = vmlaq_f32(vmulq_f32(inputVecA1, a), inputVecB1, b);
            const auto result2 = vmlaq_f32(vmulq_f32(inputVecA2, a), inputVecB2, b);
            const auto result3 = vmlaq_f32(vmulq_f32(inputVecA3, a), inputVecB3, b);
            vst1q_f32(output + i, result0);
            vst1q_f32(output + i + 4, result1);
            vst1q_f32(output + i + 8, result2);
            vst1q_f32(output + i + 12, result3);
        }
#else
        for (; i + 1 <= count - 8; i += 8)
        {
            const auto leftChan0  = (input[i + 1] * (1.f - right)) + (input[i] * left);
            const auto rightChan0 = (input[i] * (1.f - left)) + (input[i + 1] * m_right);
            const auto leftChan1  = (input[i + 3] * (1.f - right)) + (input[i + 2] * left);
            const auto rightChan1 = (input[i + 2] * (1.f - left)) + (input[i + 3] * right);
            const auto leftChan2  = (input[i + 5] * (1.f - right)) + (input[i + 4] * left);
            const auto rightChan2 = (input[i + 4] * (1.f - left)) + (input[i + 5] * right);
            const auto leftChan3  = (input[i + 7] * (1.f - right)) + (input[i + 6] * left);
            const auto rightChan3 = (input[i + 6] * (1.f - left)) + (input[i + 7] * right);

            output[i]     = leftChan0;
            output[i + 1] = rightChan0;
            output[i + 2] = leftChan1;
            output[i + 3] = rightChan1;
            output[i + 4] = leftChan2;
            output[i + 5] = rightChan2;
            output[i + 6] = leftChan3;
            output[i + 7] = rightChan3;
        }
#endif
        for (; i < count; i += 2)
        {
            const auto leftChan  = (input[i + 1] * (1.f - m_right)) + (input[i] * m_left);
            const auto rightChan = (input[i] * (1.f - m_left)) + (input[i + 1] * m_right);

            output[i] = leftChan;
            output[i + 1] = rightChan;
        }

        return true;
    }

    void PanEffect::receiveFloat(int index, float value)
    {
        switch(index)
        {
            case Param::Left:
            {
                m_left = value;
            } break;

            case Param::Right:
            {
                m_right = value;
            } break;

            default:
            {
                pushError(Result::InvalidArg, "PanEffect received unknown parameter index");
            } break;
        }
    }

    void PanEffect::left(const float value)
    {
        sendFloat(Param::Left, clamp(value, 0, 1.f));
    }

    void PanEffect::right(float value)
    {
        sendFloat(Param::Right, clamp(value, 0, 1.f));
    }
} // insound
