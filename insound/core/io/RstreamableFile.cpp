#include "RstreamableFile.h"

#include <iostream>
#include <insound/core/Error.h>

namespace insound {
    RstreamableFile::RstreamableFile() : m_stream()
    {}

    RstreamableFile::~RstreamableFile() = default;

    bool RstreamableFile::open(const fs::path &filepath)
    {
        m_stream.open(filepath, std::ios::binary | std::ios::in);
        if (!m_stream.is_open())
        {
            INSOUND_PUSH_ERROR(Result::FileOpenErr, "Failed to open file");
            return false;
        }
        return true;
    }

    bool RstreamableFile::isOpen() const
    {
        return m_stream.is_open();
    }

    void RstreamableFile::close()
    {
        m_stream.close();
    }

    bool RstreamableFile::seek(const int64_t position)
    {
        m_stream.clear();
        m_stream.seekg(static_cast<std::streamoff>(position), std::ios::beg);
        const auto result = static_cast<bool>(m_stream);
        if (!result)
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "Failure during ifstream::seekg");
            return false;
        }

        return true;
    }

    int64_t RstreamableFile::size() const
    {
        const auto curPosition = m_stream.tellg();
        m_stream.seekg(0, std::ios::end);
        const auto sizePosition = m_stream.tellg();
        m_stream.seekg(curPosition, std::ios::beg);
        return sizePosition;
    }

    int64_t RstreamableFile::position() const
    {
        return m_stream.tellg();
    }

    int64_t RstreamableFile::read(uint8_t *buffer, const int64_t bytes)
    {
        if (!m_stream)
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "std::ifstream::read attempted read in bad state");
            return -1;
        }

        const auto lastPos = m_stream.tellg();
        m_stream.read(
            reinterpret_cast<char *>(buffer),
            static_cast<std::streamsize>(bytes));
        if (!m_stream) // read resulted in eof or bad bit set
        {
            return -1;
        }

        return m_stream.tellg() - lastPos;
    }

    bool RstreamableFile::isEof() const
    {
        return m_stream.eof();
    }
}
