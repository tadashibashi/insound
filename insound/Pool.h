#pragma once
#include <climits>
#include <functional>
#include <utility>
#include <vector>

#include "Error.h"

namespace insound {
    struct PoolID {
        uint32_t index;    ///< index in pool array
        uint32_t id;       ///< inner id
        uint32_t nextFree; ///< next free index
    };

    template <typename T>
    class Pool {
        struct PoolObject {
            PoolID id;
            T entity;
        };
    public:

        /// @param size    number of `T` to pool (implementation is fixed size)
        /// @param factory create a default `T`
        Pool(uint32_t size, std::function<T()> factory) :
            m_pool(), m_nextFree(0), m_idCounter(0)
        {
            m_pool.reserve(size);
            for (size_t i = 0; i < size; ++i)
            {
                m_pool.emplace_back(PoolObject{
                    .id = {
                        .index = i,
                        .nextFree = (i == size - 1) ? UINT32_MAX : i + 1
                    },
                    .entity = factory(),
                });
            }
        }

        /// Checks if there are no free entities left to get
        [[nodiscard]]
        bool empty() const
        {
            return (m_nextFree == UINT32_MAX);
        }

        /// Draw out a fresh entity from the pool
        /// @param outID     [out] pointer to receive PoolID
        /// @param outEntity [out] pointer to receive T pointer
        /// @returns whether get was successful
        bool getOne(PoolID *outID, T **outEntity)
        {
            if (empty())
            {
                pushError(Error::RuntimeErr, "No more entities left in pool");
                return false;
            }

            if (!outID || !outEntity)
            {
                pushError(Error::InvalidArg, "Either outID or outEntity are null, both must be present");
                return false;
            }

            auto &poolObj = m_pool[m_nextFree];
            poolObj.id.id == m_idCounter++;       // initialize inner id
            m_nextFree = poolObj.id.nextFree;     // move free list head to next free

            // done
            *outID = poolObj.id;
            *outEntity = &poolObj.entity;
            return true;
        }

        bool isValid(const PoolID &id, T *entity)
        {
            const auto &poolObj = m_pool[id.index];
            return (&poolObj.entity == entity) && poolObj.id.id == id.id;
        }

        /// Put an entity back into the pool
        bool putBack(const PoolID &id, T *entity)
        {
            // Ensure this entity is valid and belongs to this pool
            if (!isValid(id, entity))
            {
                pushError(Error::LogicErr, "entity does not belong to pool");
                return false;
            }

            auto &poolObj = m_pool[id.index];

            // Move this entity to the head of the free list
            const auto tempNextFree = m_nextFree;
            m_nextFree = id.index;

            m_pool[id.index].id = PoolID {
                .id = UINT32_MAX, // invalidate the inner id
                .index = id.index,
                .nextFree = tempNextFree,
            };

            return true;
        }
    private:
        std::vector<PoolObject> m_pool;
        uint32_t m_nextFree;
        uint32_t m_idCounter;
    };
}
