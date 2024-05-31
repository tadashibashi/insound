#pragma once

#include "SoundSource.h"
#include "Error.h"

namespace insound {
    template <typename T>
    class SourceHandle { // wrapper around sound source object to ensure validity
        static_assert(std::is_base_of_v<SoundSource, T>, "`T` must derive from SoundSource");
    public:
        SourceHandle() : m_source(), m_engine() { }

        explicit SourceHandle(T *source, Engine *engine) : m_source(source), m_engine(engine)
        {
        }

        T *operator->()
        {
            if (!isValid())
            {
                pushError(Error::RuntimeErr, "Attempted to access invalid source handle");
            }

            return m_source;
        }

        [[nodiscard]]
        bool isValid() const;

    private:
        T *m_source;
        Engine *m_engine;
    };


}

#include "Engine.h"

template<typename T>
bool insound::SourceHandle<T>::isValid() const
{
    return m_engine && m_engine->isSourceValid(m_source);
}
