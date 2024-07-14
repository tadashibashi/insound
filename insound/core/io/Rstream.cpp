#include "Rstream.h"
#include "Rstreamable.h"
#include "RstreamableMemory.h"

#include "../Error.h"

namespace insound {
    Rstream::~Rstream()
    {
        delete m_stream;
    }

    Rstream::Rstream(Rstream &&other) : m_stream(other.m_stream)
    {
        other.m_stream = nullptr;
    }

    Rstream &Rstream::operator=(Rstream &&other)
    {
        if (this != &other)
        {
            delete m_stream;
            m_stream = other.m_stream;
            other.m_stream = nullptr;
        }

        return *this;
    }

    bool Rstream::openFile(const std::string &filepath, bool inMemory)
    {
        Rstreamable *stream = Rstreamable::create(filepath, inMemory);
        if (!stream)
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "Failed to create Rstreamable");
            delete stream;
            return false;
        }

        m_stream = stream;
        return true;
    }

    bool Rstream::openConstMem(const uint8_t *data, size_t size)
    {
        auto stream = new RstreamableMemory();
        if (!stream->openConstMem(data, size))
        {
            delete stream;
            return false;
        }

        m_stream = stream;
        return true;
    }

    bool Rstream::openMem(uint8_t *data, size_t size, void(*deallocator)(void *data))
    {
        auto stream = new RstreamableMemory();
        if (!stream->openMem(data, size, deallocator))
        {
            delete stream;
            return false;
        }

        m_stream = stream;
        return true;
    }

    void Rstream::close()
    {
        m_stream->close();
    }

    bool Rstream::isOpen() const
    {
        return m_stream && m_stream->isOpen();
    }

    bool Rstream::seek(int64_t position)
    {
        return m_stream->seek(position);
    }

    int64_t Rstream::size() const
    {
        return m_stream->size();
    }

    int64_t Rstream::tell() const
    {
        return m_stream->tell();
    }

    bool Rstream::isEof() const
    {
        return m_stream->isEof();
    }

    int64_t Rstream::read(uint8_t *buffer, int64_t byteCount)
    {
        return m_stream->read(buffer, byteCount);
    }


}
