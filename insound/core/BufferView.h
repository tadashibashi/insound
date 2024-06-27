#pragma once

#include "util.h"

#include <string>
#include <vector>

namespace insound {

    /// Helper to view a memory buffer with both read and write functionality
    /// @warning this should only be used on known data types
    class BufferView
    {
    public:
        /// Create BufferView from a buffer
        /// @param buffer pointer to memory to view
        /// @param size size of the buffer in bytes
        /// @param endianness expected endianness of numeric data types (strings are always handled in little-endian order)
        BufferView(const void *buffer, const size_t size, const endian::type endianness = endian::little) :
            m_buf((uint8_t *)buffer), m_pos(0), m_size(size), m_endian(endianness)
        {}

        /// Create BufferView from null-terminated c-string
        /// @param buffer c string with memory to view
        /// @param endianness expected endianness of numeric data types (strings are always handled in little-endian order)
        explicit BufferView(const char *buffer, const endian::type endianness = endian::little);

        /// Create BufferView from string of byte data
        /// @param buffer string containing memory to view
        /// @param endianness expected endianness of numeric data types (string are always handled in little-endian order)
        explicit BufferView(const std::string &buffer, const endian::type endianness = endian::little) :
            m_buf((uint8_t *)buffer.data()), m_pos(0), m_size(buffer.length()), m_endian(endianness)
        {}

        /// Create BufferView from vector of uint8_t
        /// @param buffer vector containing memory to view
        /// @param endianness expected endianness of numeric data types (strings are always handled in little-endian order)
        explicit BufferView(const std::vector<uint8_t> &buffer, const endian::type endianness = endian::little) :
            m_buf((uint8_t *)buffer.data()), m_pos(0), m_size(buffer.size()), m_endian(endianness)
        {}

        /// Create BufferView from vector of bytes
        /// @param buffer vector containing memory to view
        /// @param endianness expected endianness of numeric data types (strings are always handled in little-endian order)
        explicit BufferView(const std::vector<char> &buffer, const endian::type endianness = endian::little) :
            m_buf((uint8_t *)buffer.data()), m_pos(0), m_size(buffer.size()), m_endian(endianness)
        {}

        /// Read data into a numeric value. Currently, this function only supports primitive numeric types.
        /// Use other overloads for strings.
        /// @tparam T primitive numberic type - please ensure this type's size is what you intend to read.
        /// @param outVal [out] value to receive the data read
        /// @returns number of bytes read; if > 0, `outVal` will contain the read data;
        ///          if 0, `outVal` will be unmodified. Check sdgl::getError on 0 for more details.
        template <typename T>
        size_t read(T &outVal)
        {
            return readRaw(&outVal, sizeof(T));
        }

        template <typename T>
        T getOr(T defaultVal = T())
        {
            auto result = read(defaultVal);
            assert(result);

            return defaultVal;
        }

        /// Read a null-terminated string from the buffer
        /// @param outString [out] string to receive the data
        /// @param maxSize         maximum number of characters to fill `outString`; the buffer position will still read
        ///                        until the null-terminator, even if it exceeds this value, but such characters will
        ///                        not be added to `outString`.
        ///
        /// @returns the number of bytes read (including null terminator) -
        ///          if > 0 `outString` will contain data, otherwise `outString` will remain unmodified.
        ///          Check sdgl::getError() for more information when 0 is returned.
        ///          Note: this number may differ from string length + 1 if `maxSize` clipped the out value.
        size_t read(std::string &outString, size_t maxSize = SIZE_MAX);

        /// Read a string with an expected length
        /// @param outString [out] string to receive the data
        /// @param length          length of the string in bytes (not including null terminator)
        /// @returns the number of bytes read (including null terminator) -
        ///          if > 0 `outString` will contain data, otherwise `outString` will remain unmodified.
        ///          Check sdgl::getError() for more information when 0 is returned.
        size_t readFixedString(std::string &outString, size_t length);

        /// Read a null-terminated string from the buffer
        /// @param outBuffer [out] pointer to a string buffer to receive the data
        /// @param maxSize         max length of `outBuffer`; the buffer position will still read until the
        ///                        null-terminator, even if it exceeds this value, but such characters will not be added
        ///                        to `outBuffer`.
        ///
        /// @returns the number of bytes read (including null terminator) -
        ///          if > 0 `outBuffer` will contain data, otherwise `outBuffer` will remain unmodified.
        ///          Check sdgl::getError for more info when 0 is returned.
        ///          Note: this number may differ from string length + 1 if `maxSize` clipped the out value.
        size_t read(char *outBuffer, size_t maxSize);


        /// Peek relative to the current location. In debug mode, an assertion is made to check bounds.
        /// @param offset - offset bytes, may be negative
        ///
        /// @returns value of byte at location
        [[nodiscard]]
        uint8_t peek(int offset = 0) const;

        /// Move the view's position back to 0
        void reset()
        {
            m_pos = 0;
        }

        /// Get the buffer size
        [[nodiscard]]
        auto size() const { return m_size; }

        /// Get the number of bytes left to read (from current position to the end of the buffer)
        [[nodiscard]]
        auto bytesLeft() const { return m_size - m_pos; }

        /// Get the view's byte position
        [[nodiscard]]
        auto position() const { return m_pos; }

        /// Get the pointer to the data (at position 0)
        [[nodiscard]]
        const void *data() const { return m_buf; }

        /// Get the expected endianness that was set in the view's constructor
        [[nodiscard]]
        auto expectedEndian() const { return m_endian; }

        void move(size_t offset)
        {
            m_pos = offset;
        }

        size_t readRaw(void *buffer, size_t size);
    protected:
        uint8_t *m_buf;
        size_t m_pos;
        size_t m_size;
        endian::type m_endian;
    };

    class BufferWriter : public BufferView
    {
    public:
        BufferWriter(std::string &buffer, endian::type endianness) :
            BufferView(buffer.data(), buffer.size(), endianness) { }

        template <typename T>
        size_t write(T value)
        {
            return writeImpl(&value, sizeof(T));
        }
        /// Write null-terminated string into the buffer
        /// @note make sure there is enough space in the buffer for a null-terminator
        /// @returns the number of bytes written, this includes the string's null terminator
        size_t write(const std::string &str);

        /// Write string into the buffer
        /// @note make sure there is enough space for a null-terminator
        /// @returns the number of bytes read (including null terminator)
        size_t write(std::string_view str);

        size_t write(const char *str, size_t length);
    private:
        size_t writeImpl(const void *data, size_t size);
    };
}
