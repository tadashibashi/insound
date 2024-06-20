#include "VolumeEffect.h"

#include "../CpuIntrinsics.h"
#include "../Error.h"

namespace insound {
    bool VolumeEffect::process(const float *input, float *output, int count)
    {
        const auto volume = m_volume;

        if (volume == 1.f)
            return false;

#if     INSOUND_SSE
        int i = 0;
        const auto volumes = _mm_set1_ps(volume);
        for(; i <= count - 16; i += 16)
        {
            auto a = _mm_load_ps(input + i);
            auto b = _mm_load_ps(input + i + 4);
            auto c = _mm_load_ps(input + i + 8);
            auto d = _mm_load_ps(input + i + 12);
            auto resultA = _mm_mul_ps(a, volumes);
            auto resultB = _mm_mul_ps(b, volumes);
            auto resultC = _mm_mul_ps(c, volumes);
            auto resultD = _mm_mul_ps(d, volumes);
            _mm_store_ps(output + i, resultA);
            _mm_store_ps(output + i + 4, resultB);
            _mm_store_ps(output + i + 8, resultC);
            _mm_store_ps(output + i + 12, resultD);
        }

#elif   INSOUND_WASM_SIMD
        int i = 0;
        const auto volumes = wasm_f32x4_splat(m_volume);
        for(; i <= count - 16; i += 16)
        {
            auto a = wasm_v128_load(input + i);
            auto b = wasm_v128_load(input + i + 4);
            auto c = wasm_v128_load(input + i + 8);
            auto d = wasm_v128_load(input + i + 12);
            auto resultA = wasm_f32x4_mul(a, volumes);
            auto resultB = wasm_f32x4_mul(b, volumes);
            auto resultC = wasm_f32x4_mul(c, volumes);
            auto resultD = wasm_f32x4_mul(d, volumes);
            wasm_v128_store(output + i, resultA);
            wasm_v128_store(output + i + 4, resultB);
            wasm_v128_store(output + i + 8, resultC);
            wasm_v128_store(output + i + 12, resultD);
        }
#elif INSOUND_ARM_NEON
        int i = 0;
        float32x4_t volumes = vdupq_n_f32(volume);
        for (; i <= count - 16; i += 16)
        {
            float32x4_t a = vld1q_f32(input + i);
            float32x4_t b = vld1q_f32(input + i + 4);
            float32x4_t c = vld1q_f32(input + i + 8);
            float32x4_t d = vld1q_f32(input + i + 12);
            float32x4_t resultA = vmulq_f32(a, volumes);
            float32x4_t resultB = vmulq_f32(b, volumes);
            float32x4_t resultC = vmulq_f32(c, volumes);
            float32x4_t resultD = vmulq_f32(d, volumes);
            vst1q_f32(output + i, resultA);
            vst1q_f32(output + i + 4, resultB);
            vst1q_f32(output + i + 8, resultC);
            vst1q_f32(output + i + 12, resultD);
        }
#else
        int i = 0;
        for (; i <= count - 16; i += 16)
        {
            output[i] = input[i] * volume;
            output[i+1] = input[i+1] * volume;
            output[i+2] = input[i+2] * volume;
            output[i+3] = input[i+3] * volume;
            output[i+4] = input[i+4] * volume;
            output[i+5] = input[i+5] * volume;
            output[i+6] = input[i+6] * volume;
            output[i+7] = input[i+7] * volume;
            output[i+8] = input[i+8] * volume;
            output[i+9] = input[i+9] * volume;
            output[i+10] = input[i+10] * volume;
            output[i+11] = input[i+11] * volume;
            output[i+12] = input[i+12] * volume;
            output[i+13] = input[i+13] * volume;
            output[i+14] = input[i+14] * volume;
            output[i+15] = input[i+15] * volume;
        }
#endif
        for (; i < count; ++i)
        {
            output[i] = input[i] * volume;
        }

        return true;
    }

    void VolumeEffect::receiveFloat(int index, float value)
    {
        switch(index)
        {
            case Param::Volume:
            {
                m_volume = value;
            } break;

            default:
            {
                pushError(Result::InvalidArg, "Unknown parameter index");
            } break;
        }
    }

    void VolumeEffect::volume(float value)
    {
        sendFloat(Param::Volume, value);
    }
}
