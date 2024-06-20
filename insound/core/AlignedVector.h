#pragma once

#include "util.h"

#include <new>
#include <vector>

namespace insound {
    template <typename T, size_t AlignmentBytes>
    struct AlignedAllocator {
        using value_type = T;
        static std::align_val_t constexpr Alignment { AlignmentBytes };

        /**
         * This is only necessary because AlignedAllocator has a second template
         * argument for the alignment that will make the default
         * std::allocator_traits implementation fail during compilation.
         * @see https://stackoverflow.com/a/48062758/2191065
         */
        template<class OtherElementType>
        struct rebind
        {
            using other = AlignedAllocator<OtherElementType, AlignmentBytes>;
        };

        AlignedAllocator() noexcept = default;
        template <class U> constexpr explicit AlignedAllocator(const AlignedAllocator<U, AlignmentBytes>&) noexcept {}

        T* allocate(const size_t n)
        {
            T *ptr = static_cast<T *>(::operator new[](n * sizeof(T), Alignment));
            if (!ptr) {
                throw std::bad_alloc();
            }
            return ptr;
        }

        void deallocate(T *ptr, std::size_t) noexcept {
            ::operator delete[] (ptr, Alignment);
        }
    };

    template <typename T, size_t Alignment>
    using AlignedVector = std::vector<T, AlignedAllocator<T, Alignment>>;
}
