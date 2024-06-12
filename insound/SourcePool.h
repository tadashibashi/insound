#pragma once
#include "Pool.h"

#include <map>
#include <typeindex>
#include <unordered_map>

namespace insound {
    template <typename T>
class Handle {
    public:
        Handle() : m_id(), m_pool() { } // invalid null handle
        Handle(Pool::ID id, Pool *pool) : m_id(id), m_pool(pool) { }
        ~Handle() = default;

        [[nodiscard]]
        T *operator ->() { return (T *)m_pool->get(m_id); }
        [[nodiscard]]
        const T *operator ->() const { return (T *)m_pool->get(m_id); }

        [[nodiscard]]
        T *get() { return (T *)m_pool->get(m_id); }
        [[nodiscard]]
        const T *get() const { return m_pool->get(m_id); }

        [[nodiscard]]
        T &operator *() { return *(T *)m_pool->get(m_id); }
        [[nodiscard]]
        const T &operator *() const { return *(T *)m_pool->get(m_id); }

        template <typename U>
        [[nodiscard]]
        const U *getAs() const
        {
            return dynamic_cast<const U *>(get());
        }

        /// Dynamic cast to target type. Will return nullptr if type is not in the inheritance hierarchy.
        template <typename U>
        [[nodiscard]]
        U *getAs()
        {
            return dynamic_cast<U *>(get());
        }

        /// Check with owning pool that this handle is valid
        [[nodiscard]]
        bool isValid() const
        {
            return m_pool && m_pool->isValid(m_id);
        }

        [[nodiscard]]
        explicit operator bool() const
        {
            return static_cast<bool>(m_id);
        }

        template <typename U>
        explicit operator Handle<const U>() const
        {
            auto uptr = dynamic_cast<U *>(get());


            return uptr ? Handle<U>(m_id, m_pool) : Handle<U>();
        }

        template <typename U>
        explicit operator Handle<U>()
        {
            auto uptr = dynamic_cast<U *>(get());
            return uptr ? Handle<U>(m_id, m_pool) : Handle<U>();
        }

        template <typename U>
        [[nodiscard]]
        bool operator ==(const Handle<U> &other)
        {
            return m_id.id == other.m_id.id && m_pool == other.m_pool;
        }

        template <typename U>
        [[nodiscard]]
        bool operator !=(const Handle<U> &other)
        {
            return !operator==(other);
        }

        [[nodiscard]]
        Pool::ID id() const { return m_id; }

    private:
        friend class SourcePool;
        Pool::ID m_id;
        Pool *m_pool;
    };

    class SourcePool {
    public:
        template <typename T, typename...TArgs>
        Handle<T> allocate(TArgs &&...args)
        {
            static_assert(!std::is_abstract_v<T>, "Cannot allocate an abstract class");

            auto &pool = getPool<T>();
            auto id = pool.allocate();
            new (pool.get(id)) T(std::forward<TArgs>(args)...);

            return Handle<T>(id, &pool);
        }

        /// Deallocate a handle that was retrieved via `SourcePool::allocate`
        /// @tparam T this actually does not matter here as the handle will deallocate from its
        ///       associated pool.
        template <typename T>
        void deallocate(const Handle<T> &handle)
        {
            if (handle.isValid())
            {
                handle->~T();
                handle.m_pool->deallocate(handle.m_id);
            }

        }

        template <typename T>
        bool tryFind(T *ptr, Handle<T> *outHandle) const
        {
            static_assert(!std::is_abstract_v<T>, "Cannot find an abstract pool object");

            if (ptr == nullptr) return false;

            auto &pool = getPool<T>();

            Pool::ID id;
            if (!pool.tryFind(ptr, &id))
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
        auto &getPool(const size_t sizeIfInit = 16) const
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
            return newPool;
        }

        mutable std::unordered_map<std::type_index, int> m_indices;
        mutable std::vector<Pool *> m_poolPtrs;
        mutable std::map<std::type_index, Pool> m_pools;
    };
}


