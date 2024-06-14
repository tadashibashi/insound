#pragma once
#include <cstdint>
#include <type_traits>

namespace insound {
    float clamp(float value, float min, float max);
    int clampi(int value, int min, int max);

    /// Aligns a value to snap to a multiple of `alignment`
    uint32_t align(uint32_t alignment, uint32_t value);

    struct endian {
        enum type {
#if defined(_MSC_VER) && !defined(__clang__)
            little = 0,
            big    = 1,
            native = little,
    #else
            little = __ORDER_LITTLE_ENDIAN__,
            big    = __ORDER_BIG_ENDIAN__,
            native = __BYTE_ORDER__,
    #endif
        };

        // Swap the byte order of a numeric type
        template <typename T>
        static T swap(T u)
        {
            static_assert(std::is_arithmetic_v<T>, "Only simple arithmetic types should be endian-swapped");

            union {
                T u;
                uint8_t u8[sizeof(T)];
            } source, dest;

            source.u = u;

            for (size_t i = 0; i < sizeof(T); ++i)
                dest.u8[i] = source.u8[sizeof(T) - i - 1];

            return dest.u;
        }
    };
}
