#include "Pool.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace insound {

Pool::ID::ID(): index(SIZE_MAX), id(SIZE_MAX)
{ }

Pool::ID::operator bool() const
{
    return id != SIZE_MAX;
}

Pool::Pool(const size_t initSize, const size_t elemSize, const size_t alignment):
    m_memory(), m_meta(), m_size(), m_nextFree(),
    m_elemSize((elemSize + alignment - 1) & ~(alignment - 1)), // match byte alignment
    m_idCounter(), m_alignment(alignment)
{
    expand(initSize);
    m_nextFree = (initSize > 0) ? 0 : SIZE_MAX;
}

Pool::Pool(Pool &&other) noexcept : m_memory(other.m_memory), m_meta(other.m_meta), m_size(other.m_size),
    m_nextFree(other.m_nextFree), m_elemSize(other.m_elemSize), m_idCounter(other.m_idCounter),
    m_alignment(other.m_alignment)
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

    other.m_memory = nullptr;
    other.m_meta = nullptr;
    other.m_size = 0;
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
        m_meta[i] = Meta(ID(i, SIZE_MAX), i+1);
    }

    m_meta[newSize - 1].nextFree = SIZE_MAX;
    m_size = newSize;
}

}
