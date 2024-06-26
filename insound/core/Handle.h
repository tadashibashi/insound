#pragma once
#include "Pool.h"

namespace insound {
    template <typename T>
    class Handle {
    public:
        Handle() : m_id(), m_pool() { } // invalid null handle
        Handle(PoolID id, PoolBase *pool) : m_id(id), m_pool(pool) { }
        ~Handle() = default;

        [[nodiscard]]
        T *operator ->() const
        {
            if (!isValid())
                detail::pushSystemError(Result::InvalidHandle);
            return (T *)m_pool->get(m_id);
        }

        /// Get raw pointer, it's best to check `Handle::isValid()`
        /// first unless you are sure it's valid.
        [[nodiscard]]
        T *get() const { return (T *)m_pool->get(m_id); }

        T &operator *() const { return *(T *)m_pool->get(m_id); }

        /// Dynamic cast to target type. Will return nullptr if type is not in the inheritance hierarchy.
        template <typename U>
        [[nodiscard]]
        U *getAs() const
        {
            return dynamic_cast<U *>(get());
        }

        /// Check with owning pool that this handle is valid
        [[nodiscard]]
        bool isValid() const
        {
            // checking for `nullptr` covers default-constructed null handles
            return m_pool != nullptr && m_pool->isValid(m_id);
        }

        [[nodiscard]]
        explicit operator bool() const
        {
            return static_cast<bool>(m_id);
        }

        template <typename U>
        explicit operator Handle<U>() const
        {
            auto uptr = dynamic_cast<U *>(get());
            return uptr ? Handle<U>(m_id, m_pool) : Handle<U>();
        }

        template <typename U>
        Handle<U> cast() const
        {
            return static_cast<Handle<U>>(*this);
        }

        template <typename U>
        [[nodiscard]]
        bool operator ==(const Handle<U> &other) const
        {
            return m_id.id == other.m_id.id && m_pool == other.m_pool;
        }

        template <typename U>
        [[nodiscard]]
        bool operator !=(const Handle<U> &other) const
        {
            return !operator==(other);
        }

        [[nodiscard]]
        PoolID id() const { return m_id; }

    private:
        friend class MultiPool;
        PoolID m_id;
        mutable PoolBase *m_pool;
    };
}
