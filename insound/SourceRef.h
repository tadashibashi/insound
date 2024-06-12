#pragma once
#include "Source.h"
#include "Error.h"

//
// namespace insound {
//     template <typename T>
//     class SourceRef { // wrapper around sound source object to ensure validity
//         static_assert(std::is_base_of_v<Source, T>, "`T` must derive from Source");
//     public:
//         SourceRef() : m_handle(), m_pool() { }
//
//         SourceRef(const PoolManager::Handle &handle, Pool *pool) : m_handle(handle), m_pool(pool)
//         {
//         }
//
//         ~SourceRef() = default;
//
//         T *operator->()
//         {
//             if (!isValid())
//             {
//                 detail::pushSystemError(Result::InvalidHandle);
//             }
//
//             return (T *)m_pool->get(m_handle.id);
//         }
//
//         const T *operator->() const
//         {
//             if (!isValid())
//             {
//                 detail::pushSystemError(Result::InvalidHandle);
//             }
//
//             return (T *)m_pool->get(m_handle.id);
//         }
//
//         [[nodiscard]]
//         bool isValid() const;
//
//         /// Unsafely retrieve raw pointer
//         T *get()
//         {
//             return (T *)m_pool->get(m_handle.id);
//         }
//
//         /// Unsafely retrieve raw pointer
//         const T *get() const
//         {
//             return (T *)m_pool->get(m_handle.id);
//         }
//
//         explicit operator bool()
//         {
//             return get();
//         }
//
//         template <typename U>
//         SourceRef<U> dynamicCast()
//         {
//             static_assert(std::is_base_of_v<Source, U>, "type `U` must derive from Source");
//
//             return SourceRef<U>(m_handle, m_pool);
//         }
//
//         auto &handle()
//         {
//             return m_handle;
//         }
//
//         const auto &handle() const
//         {
//             return m_handle;
//         }
//
//         /// Get the associated pool object
//         [[nodiscard]]
//         const Pool *pool() const { return m_pool; }
//     private:
//         PoolManager::Handle m_handle;
//         Pool *m_pool;
//     };
// }
//
// template<typename T>
// bool insound::SourceRef<T>::isValid() const
// {
//     return m_pool && m_pool->isValid(m_handle.id);
// }
