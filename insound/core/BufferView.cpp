#include "BufferView.h"
#include "Error.h"

#include <cassert>
#include <cstring>

namespace insound {
    BufferView::BufferView(const char *buffer, const endian::type endianness) :
            m_buf((uint8_t *)buffer), m_pos(0), m_size(std::strlen(buffer)), m_endian(endianness)
        {}

    uint32_t BufferView::read(std::string &outString, const size_t maxSize)
    {
        // Current position already finished reading?
        if (m_pos >= m_size)
        {
            pushError(Result::EndOfBuffer, "Cannot read string from buffer because BufferView is done reading");
            return 0;
        }

        try
        {
            // find the null terminator's position (end of the string)
            size_t termPos = m_pos;
            while (termPos < m_size && termPos - m_pos < maxSize && m_buf[termPos] != 0)
            {
                ++termPos;
            }

            // copy the data
            outString = std::string(m_buf + m_pos, m_buf + termPos);

            if (termPos < m_size && m_buf[termPos] != 0) // last position not a terminator from early clipping from maxSize?
            {
                // finish until end of the string
                while(termPos < m_size && m_buf[termPos] != 0)
                    ++termPos;
            }

            m_pos = termPos + 1;
            return outString.size() + 1u; // +1 includes the null-terminator
        }
        catch(const std::exception &e)
        {
            pushError(Result::StdExcept, e.what());
            return 0;
        }
        catch(...)
        {
            pushError(Result::UnknownErr, "Failed to read string from buffer: unknown error");
            throw;
        }
    }

    uint32_t BufferView::readFixedString(std::string &outString, size_t length)
    {
        if (m_pos + length > m_size)
        {
            pushError(Result::EndOfBuffer, "Size of read exceeds buffer size");
            return 0;
        }

        outString = std::string(m_buf + m_pos, m_buf + m_pos + length);
        m_pos += length;
        return length;
    }

    uint32_t BufferView::read(char *outBuffer, const size_t maxSize)
    {
        // Current position already finished reading?
        if (m_pos >= m_size)
        {
            pushError(Result::EndOfBuffer, "Cannot read string from buffer because BufferView is done reading");
            return 0;
        }

        try
        {
            // find the null terminator's position
            size_t termPos = m_pos;
            while (termPos < m_size &&
                termPos - m_pos < maxSize - 1 &&  // must be `maxSize-1` to leave room for null terminator
                m_buf[termPos] != 0)
            {
                ++termPos;
            }

            std::memcpy(outBuffer, m_buf + m_pos, termPos - m_pos);
            outBuffer[termPos - m_pos] = 0; // set terminator in the output string

            if (termPos < m_size && m_buf[termPos] != 0) // last index not a null terminator due to early break from clipping?
            {
                while (termPos < m_size && m_buf[termPos] != 0) // finish until end of string
                    ++termPos;
            }

            auto length = termPos - m_pos + 1; // + 1 includes the null terminator
            m_pos = termPos + 1;
            return length;
        }
        catch(const std::exception &e)
        {
            pushError(Result::StdExcept, e.what());
            return 0;
        }
        catch(...)
        {
            pushError(Result::UnknownErr, "Failed to read string from buffer: unknown error");
            throw;
        }
    }



    uint8_t BufferView::peek(const int offset) const
    {
        const int index = static_cast<int>(m_pos) + offset;

        assert(index >= 0 && index < m_size);

        return m_buf[index];
    }

    uint32_t BufferView::readRaw(void *buffer, const size_t size)
    {
        if (size == 0) return 0; // edge case

        if (m_pos + size > m_size)
        {
            pushError(Result::EndOfBuffer, "Size of read exceeds buffer size");
            return 0;
        }

        if (size > 1 && endian::native != m_endian) // endianness is opposite
        {
            // Copy bytes in reverse order
            auto dest = (uint8_t *)buffer;
            auto source = (uint8_t *)(m_buf + m_pos + size - 1);
            while (source >= m_buf + m_pos)
            {
                *dest++ = *source--;
            }
        }
        else
        {
            // Copy bytes in normal order
            std::memcpy(buffer, m_buf + m_pos, size);
        }

        m_pos += size;
        return size;
    }

    uint32_t BufferWriter::writeImpl(const void *data, size_t size)
    {
        if (size == 0) return 0; // edge case

        if (m_pos + size > m_size)
        {
            pushError(Result::EndOfBuffer, "Size of read exceeds buffer size");
            return 0;
        }

        if (size > 1 && endian::native != m_endian)
        {
            // Write bytes in reverse order
            auto dest = m_buf + m_pos;
            auto source = (const uint8_t *)data + size - 1;
            while (source >= data)
            {
                *dest++ = *source--;
            }
        }
        else
        {
            std::memcpy(m_buf + m_pos, data, size);
        }

        m_pos += size;
        return size;
    }

    uint32_t BufferWriter::write(const std::string &str)
    {
        return write(str.data(), str.size());
    }

    uint32_t BufferWriter::write(const std::string_view str)
    {
        return write(str.data(), str.size());
    }

    uint32_t BufferWriter::write(const char *str, size_t length)
    {
        if (length + m_pos + 1 > m_size) // +1 for null terminator
        {
            pushError(Result::EndOfBuffer, "Size of read exceeds buffer size");
            return 0;
        }

        std::memcpy(m_buf + m_pos, str, length);
        m_buf[m_pos + length] = '\0';
        return length + 1;
    }
}
