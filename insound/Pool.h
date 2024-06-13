#pragma once
#include "Error.h"

#include <cstddef>
#include <unordered_map>

namespace insound {

/// Stores fixed blocks of memory.
/// Number of slots expands when it reaches full capacity.
/// Intended as an array for any single type of data.
class Pool {
public:
    struct ID {
        ID(const size_t index, const size_t id) : index(index), id(id) { }
        ID(); // null id
        size_t index, id;

        explicit operator bool() const;
    };

    struct Meta {
        Meta(const ID id, const size_t nextFree) : id(id), nextFree(nextFree) { }
        ID id;
        size_t nextFree;
    };

    /// @param initSize number of initial data slots
    /// @param elemSize size of one slot in bytes
    /// @param alignment byte alignment of memory
    Pool(size_t initSize, size_t elemSize, size_t alignment = alignof(std::max_align_t));

    Pool(const Pool &) = delete;
    Pool &operator=(const Pool &) = delete;

    Pool(Pool &&other) noexcept;
    Pool &operator=(Pool &&other) noexcept;

    ~Pool();

    /// Get a slot of pool data. Pool may expand if it is full.
    ID allocate();

    /// Allocate data slots ahead of time to prevent dynamic resizes
    void reserve(size_t size);

    /// Returns memory ownership back to the pool.
    /// @note user should run any cleanup logic on the memory before calling deallocate.
    ///
    /// @param id
    void deallocate(const ID &id);

    /// Check if an id returned from `allocate` is valid. Does not differentiate between ids from other pools,
    /// so user must make sure that ID is from the correct pool.
    [[nodiscard]]
    bool isValid(const ID &id) const { return id.index < m_size && m_meta[id.index].id.id == id.id; }

    /// Users of the pool should access memory via this function instead of caching pointers long-term
    /// as memory resizing can cause pointers to become invalidated should the pool be dynamically resized.
    /// Returns `nullptr` if id is invalid
    void *get(const ID &id)
    {
        if (!isValid(id))
            detail::pushSystemError(Result::InvalidHandle);
        return m_memory + id.index * m_elemSize;
    }

    bool tryFind(void *ptr, ID *outID);

    [[nodiscard]]
    const void *get(const ID &id) const { return isValid(id) ? m_memory + id.index * m_elemSize : nullptr; }

    [[nodiscard]]
    size_t maxSize() const { return m_size; }

    /// Size of one element in the pool
    [[nodiscard]]
    size_t elemSize() const { return m_elemSize; }

    /// Alignment of element type
    [[nodiscard]]
    size_t alignment() const { return m_alignment; }

    /// Deallocate all memory.
    /// Does not run any cleanup logic, though - please make sure to clean up memory before calling clear.
    void clear();

    /// Raw memory
    [[nodiscard]]
    char *data() { return m_memory; }
    /// Raw memory
    [[nodiscard]]
    const char *data() const { return m_memory; }

    [[nodiscard]]
    auto aliveCount() const { return m_aliveCount; }

    /// DO NOT USE. All handles become invalidated, and there is no solution yet.
    /// @param newSize    size to shrink to; if less than `aliveCount()`, it will use the alive count.
    /// @param outIndices map containing inner id keys to their updated indices. (optional)
    /// @returns whether shrink occurred. If current size is <= `newSize`, `false` will be returned.
    bool shrink(size_t newSize, std::unordered_map<size_t, size_t> *outIndices = nullptr);


private:
    [[nodiscard]]
    bool isFull() const;
    void expand(size_t newSize);


    char *m_memory;               ///< pointer to storage
    Meta *m_meta;                 ///< contains information on a slot of memory
    size_t m_size;                ///< current pool size

    size_t m_nextFree;            ///< next free pool index
    size_t m_elemSize;            ///< size of each memory block
    size_t m_idCounter;           ///< next id to set on `allocate`
    size_t m_alignment;           ///< memory alignment
    size_t m_aliveCount;
};

    // Structs for hashing Pool::ID in stl containers
    // e.g. std::unordered_set<Pool::ID, detail::PoolIDHash, detail::PoolIDEqual>
    namespace detail {
        struct PoolIDHash {
            [[nodiscard]]
            size_t operator()(const Pool::ID &id) const noexcept {
                return id.id;
            }
        };

        struct PoolIDEqual {
            [[nodiscard]]
            bool operator()(const Pool::ID &a, const Pool::ID &b) const noexcept
            {
                return a.id == b.id;
            }
        };
    }

}
