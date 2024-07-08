#include "../io/RstreamableFile.h"

#include <insound/core/logging.h>

#include "../Error.h"

namespace insound {
    RstreamableFile::RstreamableFile() : m_stream()
    {}

    RstreamableFile::~RstreamableFile() = default;

    bool RstreamableFile::open(const std::string &filepath)
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
        if (!m_stream.is_open())
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr,
                "std::ifstream::read attempted seek on unopened file");
            return false;
        }

        m_stream.clear();
        m_stream.seekg(static_cast<std::streamoff>(position), std::ios::beg);
        const auto result = static_cast<bool>(m_stream);
        if (m_stream.fail())
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

    int64_t RstreamableFile::tell() const
    {
        return m_stream.tellg();
    }

    int64_t RstreamableFile::read(uint8_t *buffer, const int64_t bytes)
    {
        if (!m_stream.is_open())
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr,
                "std::ifstream::read attempted read on unopened file");
            return -1;
        }

        if (m_stream.eof())
        {
            return 0;
        }

        if (m_stream.fail())
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr,
                "std::ifstream::read attempted read in bad state");
            return -1;
        }

        if (!buffer) // just perform a seek if no buffer provided
        {
            m_stream.seekg(bytes, std::ios_base::cur);

            return m_stream ? bytes : 0;
        }

        const auto lastPos = m_stream.tellg();
        m_stream.read(
            reinterpret_cast<char *>(buffer),
            static_cast<std::streamsize>(bytes));

        if (m_stream.eof())
        {
            return m_stream.gcount(); // Return bytes read before EOF
        }

        if (m_stream.fail())
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "Failed to read from file");
            return -1;
        }


        return m_stream.tellg() - lastPos;
    }

    bool RstreamableFile::isEof() const
    {
        return m_stream.eof();
    }
}
