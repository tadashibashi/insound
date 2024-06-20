#pragma once
#include "Handle.h"
#include "Pool.h"

#include <map>
#include <typeindex>
#include <unordered_map>
#include <mutex>

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
    ///
    /// Pool contains its own mutex, so that it is safe to use with multiple threads.
    class MultiPool {
    public:
        MultiPool() : m_aliveCount(0) {}
        ~MultiPool()
        {
            for (auto &[type, pool] : m_pools)
            {
                delete pool;
            }
        }
        template <typename T, typename...TArgs>
        Handle<T> allocate(TArgs &&...args)
        {
            static_assert(!std::is_abstract_v<T>, "Cannot allocate an abstract class");

            PoolBase *pool;
            PoolID id;
            uint32_t elemSize;
            {
                std::lock_guard lockGuard(m_mutex);

                pool = &getPool<T>();
                elemSize = pool->elemSize();

                // Allocate new entity
                id = pool->allocate();
            }

            try {
                // Init the newly retrieved entity
                ((T *)(pool->data() + id.index * elemSize))->init(std::forward<TArgs>(args)...); // `T` poolable must implement `init`
            }
            catch(const std::exception &err) { // init threw an exception, deallocate
                pushError(Result::RuntimeErr, err.what());
                pool->deallocate(id);
                return {};
            }
            catch(...) {
                pushError(Result::RuntimeErr, "constructor threw unknown error");
                pool->deallocate(id);
                return {};
            }

            return Handle<T>(id, pool);
        }

        /// Deallocate a handle that was retrieved via `MultiPool::allocate`
        /// @tparam T this actually does not matter here as the handle will deallocate from its
        ///           associated pool. It's just to satisfy the varied number of handles.
        /// @returns whether deallocation succeeded without problems. If false, handle was invalid, or an error was
        ///          thrown in the destructor. Check `popError()` for details. Object will still deallocate back to
        ///          the pool if if a valid handle was passed.
        template <typename T>
        bool deallocate(const Handle<T> &handle)
        {
            bool dtorThrew = false;
            if (handle.isValid())
            {
                try // catch any exception propagated from client `release()`
                {
                    handle->release();
                }
                catch(const std::exception &err)
                {
                    pushError(Result::RuntimeErr, err.what()); // don't propagate exception, just push as error
                    dtorThrew = true;
                }
                catch(...)
                {
                    pushError(Result::RuntimeErr, "Unknown exception thrown in pool object `release()`");
                    dtorThrew = true;
                }
            }
            else
            {
                pushError(Result::InvalidHandle, "Invalid handle was passed to MultiPool::deallocate");
                return false;
            }

            std::lock_guard lockGuard(m_mutex);
            handle.m_pool->deallocate(handle.m_id);

            return !dtorThrew;
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

            std::lock_guard lockGuard(m_mutex);
            if (pointer == nullptr) return false;

            auto &pool = getPool<T>();

            PoolID id;
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

            std::lock_guard lockGuard(m_mutex);
            getPool<T>(size).second.reserve(size);
        }
    private:
        template <typename T>
        auto &getPool() const
        {
            static_assert(!std::is_abstract_v<T>, "Cannot allocate an abstract class");

            if (const auto it = m_indices.find(typeid(T));
                it != m_indices.end())
            {
                return *(Pool<T> *)m_poolPtrs[it->second];
            }

            auto newPool = new Pool<T>;
            m_pools.emplace(typeid(T), newPool);
            m_indices.emplace(typeid(T), m_poolPtrs.size());
            m_poolPtrs.emplace_back(newPool);

            return *newPool;
        }

        mutable std::unordered_map<std::type_index, int> m_indices;
        mutable std::vector<PoolBase *> m_poolPtrs;
        mutable std::map<std::type_index, PoolBase *> m_pools;
        mutable size_t m_aliveCount;
        mutable std::mutex m_mutex;
    };
}

