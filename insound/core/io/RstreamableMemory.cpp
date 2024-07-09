#include "RstreamableMemory.h"
#include "openFile.h"

#include <insound/core/Error.h>
#include <insound/core/lib.h>

#ifdef INSOUND_DEBUG
#define INIT_GUARD() do { if (!isOpen()) { \
    INSOUND_PUSH_ERROR(Result::StreamNotInit, "RstreamableMemory not init"); \
    return false; \
} } while(0)
#else
#define INIT_GUARD() INSOUND_NOOP
#endif

bool insound::RstreamableMemory::open(const std::string &filepath)
{
    std::string data;
    if (!openFile(filepath, &data))
        return false;

    m_data.swap(data); // old data is cleaned up via RAII after this scope
    m_cursor = 0;
    m_eof = false;
    return true;
}

bool insound::RstreamableMemory::isOpen() const
{
    return !m_data.empty();
}

void insound::RstreamableMemory::close()
{
    m_data = {};   // old data is cleaned up via RAII
    m_cursor = 0;
    m_eof = false;
}

bool insound::RstreamableMemory::seek(int64_t position)
{
    INIT_GUARD();

    if (position > m_data.size() || position < 0) // allow max range since ifstream::seekg allows it to read file size
    {
        INSOUND_PUSH_ERROR(Result::RangeErr, "seek position is out of range");
        return false;
    }

    m_eof = false;      // reset eof "bit"
    m_cursor = position;
    return true;
}

int64_t insound::RstreamableMemory::size() const
{
    return static_cast<int64_t>(m_data.size());
}

int64_t insound::RstreamableMemory::tell() const
{
    return m_cursor;
}

int64_t insound::RstreamableMemory::read(uint8_t *buffer, int64_t requestedBytes)
{
    if (requestedBytes == 0)  // no bytes to read == noop
        return 0;

    // End-of-file check
    if (m_eof)                // we're already at eof
        return 0;

    const auto dataSize = static_cast<int64_t>(m_data.size());
    if (m_cursor >= dataSize) // no more data to read == eof
    {
        m_eof = true;
        m_cursor = dataSize;
        return 0; // eof
    }

    // Check for errors
    if (m_cursor < 0) // it shouldn't happen, but just in case
    {
        INSOUND_PUSH_ERROR(Result::RuntimeErr, "cursor is at an invalid position");
        return -1;
    }

    if (requestedBytes < 0)
    {
        INSOUND_PUSH_ERROR(Result::InvalidArg, "invalid requestedBytes, must be >= 0");
        return -1;
    }

    // Copy bytes
    const int64_t bytesToRead = std::min(requestedBytes, dataSize - m_cursor);
    std::memcpy(buffer, m_data.data() + m_cursor, bytesToRead);

    // Reads past the end of file (but not at the end) results in setting the eof flag
    if (bytesToRead < requestedBytes)
    {
        m_eof = true;
    }

    m_cursor += bytesToRead;
    return bytesToRead;
}

bool insound::RstreamableMemory::isEof() const
{
    return m_eof;
}
