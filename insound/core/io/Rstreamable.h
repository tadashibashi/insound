#pragma once

#include <cstdint>
#include <string>

namespace insound {

    /// Low level resource stream: an abstraction around read-only file streams
    class Rstreamable {
    public:
        /// Create and open an Rstreamable by platform. Returns null on error.
        static Rstreamable *create(const std::string &filepath, bool inMemory = false);
        virtual ~Rstreamable() = default;

        /// Open the Rstreamable from a file system path.
        /// TODO: URL paths via curl or other client lib.
        /// @param filepath path to open
        /// @returns whether data source was successfully opened.
        virtual bool openFile(const std::string &filepath) = 0;

        /// Check if the Rstreamable is currently open.
        [[nodiscard]]
        virtual bool isOpen() const = 0;

        /// Close the stream. Safe to call, even if already closed.
        virtual void close() = 0;

        /// Seek to a position in the file stream relative to the starting point.
        virtual bool seek(int64_t position) = 0;

        /// Get the size of the stream, if available.
        /// Returns -1 if stream size is not determinable.
        [[nodiscard]]
        virtual int64_t size() const = 0;

        /// Tell the current read position in the stream relative to the starting point.
        [[nodiscard]]
        virtual int64_t tell() const = 0;

        /// Copy bytes from the stream into a provided buffer.
        /// @returns -1 an error occurred, if 0, eof, otherwise it will return the number of bytes read.
        virtual int64_t read(uint8_t *buffer, int64_t bytes) = 0;

        /// Whether eof flag has been set. This occurs when a read past the end has occurred.
        /// If a read reaches the end, eof will not be set until the next read.
        [[nodiscard]]
        virtual bool isEof() const = 0;
    };
}
