#pragma once
#include "Error.h"

#include <cstddef>
#include <functional>
#include <unordered_map>

namespace insound {

struct PoolID {
    PoolID(const size_t index, const size_t id) : index(index), id(id) { }
    PoolID(); // null id
    size_t index, id;

    explicit operator bool() const;

    /// To be used with a hashing class to enable PoolID hashing
    struct Hasher {
        [[nodiscard]]
        size_t operator()(const PoolID &id) const noexcept {
            return id.id;
        }
    };

    /// To be used with a hashing class to enable PoolID hashing
    struct Equals {
        [[nodiscard]]
        bool operator()(const PoolID &a, const PoolID &b) const noexcept
        {
            return a.id == b.id;
        }
    };
};

/// Stores fixed blocks of memory.
/// Number of slots expands when it reaches full capacity.
/// Intended as an array for any single type of data.

class PoolBase {
public:

    struct Meta {
        Meta(const PoolID id, const size_t nextFree) : id(id), nextFree(nextFree) { }
        PoolID id;
        size_t nextFree;
    };

    /// @param elemSize size of one slot in bytes
    explicit PoolBase(size_t elemSize);
    PoolBase(const PoolBase &) = delete;
    PoolBase &operator=(const PoolBase &) = delete;
    PoolBase(PoolBase &&other) noexcept;
    PoolBase &operator=(PoolBase &&other) noexcept;

    virtual ~PoolBase();

    /// Get a slot of pool data. Pool may expand if it is full.
    PoolID allocate();

    /// Allocate data slots ahead of time to prevent dynamic resizes
    void reserve(size_t size);

    /// Returns memory ownership back to the pool.
    /// @note user should run any cleanup logic on the memory before calling deallocate.
    ///
    /// @param id
    void deallocate(const PoolID &id);

    /// Check if an id returned from `allocate` is valid. Does not differentiate between ids from other pools,
    /// so user must make sure that PoolID is from the correct pool.
    [[nodiscard]]
    bool isValid(const PoolID &id) const { return id.index < m_size && m_meta[id.index].id.id == id.id; }

    /// Users of the pool should access memory via this function instead of caching pointers long-term
    /// as memory resizing can cause pointers to become invalidated should the pool be dynamically resized.
    /// Returns `nullptr` if id is invalid
    void *get(const PoolID &id)
    {
        if (!isValid(id))
            detail::pushSystemError(Result::InvalidHandle);
        return m_memory + id.index * m_elemSize;
    }

    bool tryFind(void *ptr, PoolID *outID);

    [[nodiscard]]
    const void *get(const PoolID &id) const { return isValid(id) ? m_memory + id.index * m_elemSize : nullptr; }

    [[nodiscard]]
    size_t maxSize() const { return m_size; }

    /// Size of one element in the pool
    [[nodiscard]]
    size_t elemSize() const { return m_elemSize; }

    /// Deallocate all memory.
    /// Does not run any cleanup logic, though - please make sure to clean up memory before calling clear.
    void clear();

    /// Raw memory
    [[nodiscard]]
    char *data() { return m_memory; }
    /// Raw memory
    [[nodiscard]]
    const char *data() const { return m_memory; }

    // /// DO NOT USE. All handles become invalidated, and there is no solution yet.
    // /// @param newSize    size to shrink to; if less than `aliveCount()`, it will use the alive count.
    // /// @param outIndices map containing inner id keys to their updated indices. (optional)
    // /// @returns whether shrink occurred. If current size is <= `newSize`, `false` will be returned.
    // bool shrink(size_t newSize, std::unordered_map<size_t, size_t> *outIndices = nullptr);

protected:
    [[nodiscard]]
    bool isFull() const;

    virtual void expand(size_t newSize) = 0;

    char *m_memory;               ///< pointer to storage
    Meta *m_meta;                 ///< contains information on a slot of memory
    size_t m_size;                ///< current pool size
    size_t m_nextFree;            ///< next free pool index
    size_t m_elemSize;            ///< size of each memory block
    size_t m_idCounter;           ///< next id to set on `allocate`
};

// Implements type safety for non-trivial data types by calling move constructor on memory reallocation
template <typename T>
class Pool final : public PoolBase {
public:
    explicit Pool() : PoolBase(sizeof(T)) { }
    ~Pool() override
    {
        for (auto ptr = (T *)m_memory, end = (T *)m_memory + m_size; ptr != end; ++ptr)
        {
            ptr->~T();
        }
    }

    void expand(size_t newSize) override
    {
        const auto lastSize = m_size;
        if (lastSize >= newSize) // no need to expand if new size isn't greater
            return;

        if (m_memory == nullptr)
        {
            m_memory = (char *)std::malloc(newSize * sizeof(T));
            m_meta = (Meta *)std::malloc(newSize * sizeof(Meta));
        }
        else
        {
            auto temp = (char *)std::malloc(newSize * sizeof(T));

            for (T *ptr = (T *)m_memory, *end = (T *)m_memory + lastSize, *target = (T *)temp; ptr != end; ++ptr, ++target)
            {
                new (target) T(std::move(*ptr));
                ptr->~T();
            }

            std::free(m_memory);
            m_memory = temp;

            auto metaTemp = (Meta *)std::malloc(newSize * sizeof(Meta));
            std::memcpy(metaTemp, m_meta, lastSize * sizeof(Meta));
            std::free(m_meta);
            m_meta = metaTemp;
        }

        // TODO: unroll these loops?
        // Initialize new meta objects
        for (size_t i = lastSize; i < newSize; ++i)
        {
            new (m_meta + i) Meta(PoolID(i, SIZE_MAX), i+1);
            new ((T *)m_memory + i) T();
        }

        m_meta[newSize - 1].nextFree = SIZE_MAX;
        m_size = newSize;
    }
};

}
