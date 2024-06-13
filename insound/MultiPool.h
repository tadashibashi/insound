#pragma once
#include "Handle.h"
#include "Pool.h"

#include <map>
#include <typeindex>
#include <unordered_map>

namespace insound {
    /// Pool that manages storage of any number of types of concrete objects.
    ///
    /// A poolable class must contain two functions:
    /// - `bool init(TArgs args...)`
    /// - `void release()`
    ///
    /// When `MultiPool::allocate` is called, the pool class's `init` is called.
    /// The args of `allocate` are forwarded to the poolable object's `init`.
    /// When the pool object is deallocated, `release` is called (with no arguments).
    ///
    /// These two functions are the setup and cleanup functions for your pooled objects.
    /// Actual construction occurs during pool initialization and expansion when an allocation exceeds
    /// the current size of a given pool and new elements are dynamically added.
    /// The actual destructor is called when the pool is deleted.
    ///
    /// For subclasses, make sure init and release calls its parent init and release if this is important.
    /// Whether they are virtual or not is up to you.
    class MultiPool {
    public:
        MultiPool() : m_aliveCount(0) {}
        ~MultiPool()
        {
            for (auto &[type, pool] : m_pools)
            {
                // destruct each element before memory is deleted
                if (const auto &dtor = m_dtors[type])
                {
                    for (size_t i = 0, size = pool.maxSize(); i < size; ++i)
                    {
                        dtor(pool.data() + i * pool.elemSize());
                    }
                }
            }
        }

        template <typename T, typename...TArgs>
        Handle<T> allocate(TArgs &&...args)
        {
            static_assert(!std::is_abstract_v<T>, "Cannot allocate an abstract class");

            auto &pool = getPool<T>();
            const auto lastSize = pool.maxSize();
            const auto elemSize = pool.elemSize();

            // Allocate new entity
            auto id = pool.allocate();

            // If pool expanded, place new objects
            if (const auto curSize = pool.maxSize(); lastSize < curSize)
            {
                for (size_t i = lastSize; i < curSize; ++i)
                {
                    new (pool.data() + i * elemSize) T();
                }
            }

            // Init the newly retrieved entity
            ((T *)(pool.data() + id.index * elemSize))->init(std::forward<TArgs>(args)...); // `T` poolable must implement `init`

            return Handle<T>(id, &pool);
        }

        /// Deallocate a handle that was retrieved via `MultiPool::allocate`
        /// @tparam T this actually does not matter here as the handle will deallocate from its
        ///           associated pool. It's just to satisfy the varied number of handles.
        template <typename T>
        void deallocate(const Handle<T> &handle)
        {
            // Only need to clean up handle if it's valid
            if (handle.isValid())
            {
                // `T::release` cleans up the object without destructing it
                handle->release();
                handle.m_pool->deallocate(handle.m_id);
            }
        }

        /// Try to find a handle from a raw pointer. Type `T` should be concrete, targeting
        /// the actual final class (as opposed to a class up its hierarchy.
        /// @param pointer   the pooled object pointer to query
        /// @param outHandle [out] pointer to retrieve the handle if found
        /// @returns whether a handle was found for the pointer.
        template <typename T>
        bool tryFind(T *pointer, Handle<T> *outHandle) const
        {
            static_assert(!std::is_abstract_v<T>, "Cannot find an abstract pool object");

            if (pointer == nullptr) return false;

            auto &pool = getPool<T>();

            Pool::ID id;
            if (!pool.tryFind(pointer, &id))
                return false;

            if (outHandle)
                *outHandle = Handle<T>(id, &pool);
            return true;
        }

        /// Reserve memory for a specific non-abstract Source subclass
        /// @param size number of `T` elements to reserve space for
        template <typename T>
        void reserve(const size_t size)
        {
            static_assert(!std::is_abstract_v<T>, "Cannot reserve space for an abstract class");
            getPool<T>(size).second.reserve(size);
        }
    private:
        template <typename T>
        auto &getPool(const size_t sizeIfInit = 8) const
        {
            static_assert(!std::is_abstract_v<T>, "Cannot allocate an abstract class");

            if (const auto it = m_indices.find(typeid(T));
                it != m_indices.end())
            {
                return *m_poolPtrs[it->second];
            }

            auto &newPool = m_pools.emplace(typeid(T), Pool(sizeIfInit, sizeof(T), alignof(T))).first->second;
            m_indices.emplace(typeid(T), m_poolPtrs.size());
            m_poolPtrs.emplace_back(&newPool);
            m_dtors.emplace(typeid(T), [](void *ptr) {
                ((T *)ptr)->~T();
            });

            // instantiate new items in new pool memory
            for (size_t i = 0; i < sizeIfInit; ++i)
            {
                new (newPool.data() + i * newPool.elemSize()) T(); // must have default ctor
            }
            return newPool;
        }

        mutable std::unordered_map<std::type_index, std::function<void(void *)>> m_dtors;
        mutable std::unordered_map<std::type_index, int> m_indices;
        mutable std::vector<Pool *> m_poolPtrs;
        mutable std::map<std::type_index, Pool> m_pools;
        mutable size_t m_aliveCount;
    };
}


