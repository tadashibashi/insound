#include "Pool.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

namespace insound {

Pool::ID::ID(): index(SIZE_MAX), id(SIZE_MAX)
{ }

Pool::ID::operator bool() const
{
    return id != SIZE_MAX;
}

Pool::Pool(const size_t initSize, const size_t elemSize, const size_t alignment):
    m_memory(), m_meta(), m_size(), m_nextFree(),
    m_elemSize(), // match byte alignment
    m_idCounter(), m_alignment(std::max(alignment, alignof(void *))), m_aliveCount(0)
{
    m_elemSize = (elemSize + m_alignment - 1) & ~(m_alignment - 1);
    expand(initSize);
    m_nextFree = (initSize > 0) ? 0 : SIZE_MAX;
}

Pool::Pool(Pool &&other) noexcept : m_memory(other.m_memory), m_meta(other.m_meta), m_size(other.m_size),
    m_nextFree(other.m_nextFree), m_elemSize(other.m_elemSize), m_idCounter(other.m_idCounter),
    m_alignment(other.m_alignment), m_aliveCount(other.m_aliveCount)
{
    other.m_memory = nullptr;
    other.m_meta = nullptr;
    other.m_size = 0;
}

Pool &Pool::operator=(Pool &&other) noexcept
{
    m_memory = other.m_memory;
    m_meta = other.m_meta;
    m_size = other.m_size;
    m_nextFree = other.m_nextFree;
    m_elemSize = other.m_elemSize;
    m_idCounter = other.m_idCounter;
    m_alignment = other.m_alignment;
    m_aliveCount = other.m_aliveCount;

    other.m_memory = nullptr;
    other.m_meta = nullptr;
    other.m_size = 0;
    other.m_aliveCount = 0;
    return *this;
}

Pool::~Pool()
{
    std::free(m_memory);
    std::free(m_meta);
}

Pool::ID Pool::allocate()
{
    if (isFull())
    {
        const auto lastSize = m_size;
        expand(lastSize * 2 + 1);

        m_nextFree = lastSize;
    }

    ++m_aliveCount;
    auto &meta = m_meta[m_nextFree];
    m_nextFree = meta.nextFree;
    meta.id.id = m_idCounter++;

    return meta.id;
}

void Pool::reserve(size_t size)
{
    expand(size);
}

void Pool::deallocate(const ID &id)
{
    if (!isValid(id))
        return;

    --m_aliveCount;
    auto &meta = m_meta[id.index];
    meta.nextFree = m_nextFree;
    meta.id.id = SIZE_MAX;
    m_nextFree = id.index;
}

bool Pool::tryFind(void *ptr, ID *outID)
{
    if (ptr < m_memory || ptr >= m_memory + m_elemSize * m_size)
        return false;
    const auto index = ((char *)ptr - m_memory) / m_elemSize;

    if (outID)
        *outID = m_meta[index].id;
    return true;
}

void Pool::clear()
{
    const auto size = m_size;
    if (size == 0) return;

    for (size_t i = 0; i < size; ++i)
    {
        auto &meta = m_meta[i];
        meta.id.id = SIZE_MAX;
        meta.nextFree = i + 1;
    }

    m_meta[size - 1].nextFree = SIZE_MAX;
    m_nextFree = 0;
}

bool Pool::shrink(size_t newSize, std::unordered_map<size_t, size_t> *outIndices)
{
    if (newSize >= m_size) // no need to shrink if bigger
        return false;

    if (newSize < m_aliveCount) // new size must be greater to or equal to the number of alive entities
        newSize = m_aliveCount;

    // allocate new memory
    auto newMemory = (char *)std::aligned_alloc(m_alignment, m_elemSize * newSize);

    constexpr auto metaSize = (sizeof(Meta) + alignof(Meta) - 1) & ~(alignof(Meta) - 1);
    auto newMeta = (Meta *)std::aligned_alloc(alignof(Meta), metaSize * newSize);

    // defragment/copy into new memory
    size_t metaCount = 0;
    for (size_t i = 0; i < m_size; ++i)
    {
        auto &oldMeta = m_meta[i];
        if (oldMeta.id.id != SIZE_MAX) // valid id
        {
            new (newMeta + metaCount) Meta(ID(metaCount, oldMeta.id.id), metaCount + 1);
            std::memcpy(newMemory + m_elemSize * metaCount, m_memory + m_elemSize * oldMeta.id.index, m_elemSize);
            ++metaCount;
        }
    }
    newMeta[metaCount-1].nextFree = SIZE_MAX;

    assert(metaCount == m_aliveCount);

    if (outIndices)
    {
        std::unordered_map<size_t, size_t> indices;
        for (size_t i = 0; i < metaCount; ++i)
        {
            const auto &meta = newMeta[i];
            indices.emplace(meta.id.id, meta.id.index);
        }

        outIndices->swap(indices);
    }

    // done, commit changes
    std::free(m_memory);
    std::free(m_meta);
    m_memory = newMemory;
    m_meta = newMeta;
    m_size = newSize;
    return true;
}

bool Pool::isFull() const { return m_nextFree == SIZE_MAX; }

void Pool::expand(size_t newSize)
{
    const auto lastSize = m_size;
    if (lastSize >= newSize) // no need to expand if new size isn't greater
        return;

    if (m_memory == nullptr)
    {
        m_memory = (char *)std::aligned_alloc(m_alignment, newSize * m_elemSize);

        m_meta = (Meta *)std::aligned_alloc(alignof(Meta), newSize * sizeof(Meta));
    }
    else
    {
        auto temp = (char *)std::aligned_alloc(m_alignment, newSize * m_elemSize);
        std::memcpy(temp, m_memory, lastSize * m_elemSize);
        std::free(m_memory);
        m_memory = temp;

        auto metaTemp = (Meta *)std::aligned_alloc(alignof(Meta), newSize * sizeof(Meta));
        std::memcpy(metaTemp, m_meta, lastSize * sizeof(Meta));
        std::free(m_meta);
        m_meta = metaTemp;
    }

    for (size_t i = lastSize; i < newSize; ++i)
    {
        new (m_meta + i) Meta(ID(i, SIZE_MAX), i+1);
    }

    m_meta[newSize - 1].nextFree = SIZE_MAX;
    m_size = newSize;
}

}
