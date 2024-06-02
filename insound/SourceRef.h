#pragma once

#include "SoundSource.h"
#include "Error.h"

namespace insound {
    template <typename T>
    class SourceRef { // wrapper around sound source object to ensure validity
        static_assert(std::is_base_of_v<SoundSource, T>, "`T` must derive from SoundSource");
    public:
        SourceRef() : m_source(), m_engine() { }

        explicit SourceRef(std::shared_ptr<T> &source, const Engine *engine) : m_source(source), m_engine(engine)
        {
        }

        T *operator->()
        {
            if (!isValid())
            {
                detail::pushSystemError(Error::InvalidHandle);
            }

            return m_source.get();
        }

        const T *operator->() const
        {
            if (!isValid())
            {
                detail::pushSystemError(Error::InvalidHandle);
            }

            return m_source.get();
        }

        [[nodiscard]]
        bool isValid() const;

        /// Unsafely retrieve raw pointer
        T *get()
        {
            return m_source.get();
        }

        /// Unsafely retrieve raw pointer
        const T *get() const
        {
            return m_source.get();
        }
    private:
        std::shared_ptr<T> m_source;
        const Engine *m_engine;
    };


}

#include "Engine.h"

template<typename T>
bool insound::SourceRef<T>::isValid() const
{
    return m_source && m_engine->isSourceValid(std::dynamic_pointer_cast<SoundSource>(m_source));
}
