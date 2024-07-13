#pragma once

#include <cstdint>
#include <string>

namespace insound {

    class Rstream {
    public:
        Rstream() : m_stream() { }

        ~Rstream();

        Rstream(Rstream &&other);
        Rstream &operator=(Rstream &&other);

        /// Open a file to begin streaming.
        /// @param filepath path to the file to open
        /// @param inMemory whether to load all file data in memory. On true, data is loaded into memory and
        ///        streamed from that memory; on false, data is streamed directly from file. (default: false)
        bool openFile(const std::string &filepath, bool inMemory = false);

        /// Read memory of file data that has been opened, but maintain ownership of data.
        /// @param data pointer to the data
        /// @param size size of the data in bytes
        /// @returns whether function call was successful.
        bool openConstMem(const uint8_t *data, size_t size);

        /// Pass in memory of file data that has been opened, handing over ownership to the stream.
        /// @param data pointer to the data
        /// @param size size of the data in bytes
        /// @param deallocator callback called to clean up data on close. Default calls `std::free` on data pointer.
        /// @returns whether function call was successful.
        bool openMem(uint8_t *data, size_t size, void(*deallocator)(void *data) = nullptr);

        [[nodiscard]]
        bool isOpen() const;
        void close();

        bool seek(int64_t position);

        [[nodiscard]]
        int64_t size() const;

        [[nodiscard]]
        int64_t tell() const;

        [[nodiscard]]
        bool isEof() const;

        int64_t read(uint8_t *buffer, int64_t byteCount);

        class Rstreamable *stream() { return m_stream; }

    private:
        class Rstreamable *m_stream;
    };


}
