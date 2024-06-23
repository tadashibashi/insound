#include "Pool.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

namespace insound {

PoolID::PoolID(): index(SIZE_MAX), id(SIZE_MAX)
{ }

PoolID::operator bool() const
{
    return id != SIZE_MAX;
}


PoolBase::PoolBase(const size_t elemSize) :
           m_memory(), m_meta(), m_size(), m_nextFree(),
           m_elemSize(elemSize), // match byte alignment
           m_idCounter()
{
    m_nextFree = SIZE_MAX;
}

PoolBase::PoolBase(PoolBase &&other) noexcept : m_memory(other.m_memory), m_meta(other.m_meta), m_size(other.m_size),
    m_nextFree(other.m_nextFree), m_elemSize(other.m_elemSize), m_idCounter(other.m_idCounter)
{
    other.m_memory = nullptr;
    other.m_meta = nullptr;
    other.m_size = 0;
}

PoolBase &PoolBase::operator=(PoolBase &&other) noexcept
{
    // clean up existing memory
    std::free(m_memory);
    std::free(m_meta);

    m_memory = other.m_memory;
    m_meta = other.m_meta;
    m_size = other.m_size;
    m_nextFree = other.m_nextFree;
    m_elemSize = other.m_elemSize;
    m_idCounter = other.m_idCounter;

    other.m_memory = nullptr;
    other.m_meta = nullptr;
    other.m_size = 0;
    return *this;
}

PoolBase::~PoolBase()
{
    std::free(m_memory);
    std::free(m_meta);
}

PoolID PoolBase::allocate()
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

void PoolBase::reserve(size_t size)
{
    const auto lastSize = m_size;
    expand(size);
    if (m_nextFree == SIZE_MAX)
        m_nextFree = lastSize;
}

void PoolBase::deallocate(const PoolID &id)
{
    if (!isValid(id))
        return;

    auto &meta = m_meta[id.index];
    meta.nextFree = m_nextFree;
    meta.id.id = SIZE_MAX;
    m_nextFree = id.index;
}

bool PoolBase::tryFind(void *ptr, PoolID *outID)
{
    if (ptr < m_memory || ptr >= m_memory + m_elemSize * m_size)
        return false;
    const auto index = ((char *)ptr - m_memory) / m_elemSize;

    if (outID)
        *outID = m_meta[index].id;
    return true;
}

void PoolBase::clear()
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

bool PoolBase::isFull() const { return m_nextFree == SIZE_MAX; }

}
