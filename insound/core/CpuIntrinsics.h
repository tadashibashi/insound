#pragma once

#if __APPLE__
#include <TargetConditionals.h>
#if TARGET_CPU_X86_64

#endif
#endif

#if INSOUND_CPU_INTRINSICS

#if __ARM_NEON__                  // ARM Neon
#define INSOUND_ARM_NEON 1
#include <arm_neon.h>

inline float32x4_t vdivq_f32_approx(float32x4_t a, float32x4_t b) {
    // Reciprocal estimate
    float32x4_t reciprocal = vrecpeq_f32(b);

    // One iteration of Newton-Raphson refinement
    reciprocal = vmulq_f32(vrecpsq_f32(b, reciprocal), reciprocal);

    // Perform division as multiplication by the reciprocal
    return vmulq_f32(a, reciprocal);
}

#endif

#if __EMSCRIPTEN__                // Wasm Simd
#define INSOUND_WASM_SIMD 1
#include <wasm_simd128.h>
#endif

#ifdef __AVX__                    // Intel
#define INSOUND_AVX 1
#include <immintrin.h>
#endif

#ifdef __SSE__
#define INSOUND_SSE 1
#include <xmmintrin.h>
#endif

#endif
