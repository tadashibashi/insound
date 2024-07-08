#include "Rstream.h"
#include "Rstreamable.h"
#include "RstreamableFile.h"
#include "RstreamableAAsset.h"
#include "../Error.h"

namespace insound {
    Rstream::~Rstream()
    {
        delete m_stream;
    }

    bool Rstream::open(const std::string &filepath, bool inMemory)
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
