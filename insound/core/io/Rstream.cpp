#include "Rstream.h"
#include "Rstreamable.h"
#include "RstreamableFile.h"
#include "RstreamableAAsset.h"
#include <insound/core/Error.h>

namespace insound {
    Rstream::~Rstream()
    {
        delete m_stream;
    }

    bool Rstream::open(const fs::path &filepath)
    {
        Rstreamable *stream = nullptr;
#ifdef __ANDROID__

        if (filepath.is_absolute())
        {
            stream = new RstreamableFile();
        }
        else
        {
            stream = new RstreamableAAsset();
        }
#else
        stream = new RstreamableFile();
#endif
        if (!stream)
        {
            pushError(Result::RuntimeErr, "Failed to create Rstreamable");
            delete stream;
            return false;
        }

        if (!stream->open(filepath))
        {
            pushError(Result::RuntimeErr, "Failed to open Rstreamable");
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

    int64_t Rstream::position() const
    {
        return m_stream->position();
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
