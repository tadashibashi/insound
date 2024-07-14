#pragma once

#include <cstdint>
#include <string>

namespace insound {
    /// A wrapper around an Rstreamable that provides a unified interface for streaming data, whether directly
    /// from a file, memory pointer, or Android APK etc. It handles backend platform details automatically.
    /// For platforms with slow disk access, or no disk access, it gives you the option to copy the data
    /// into memory first and stream from that memory instead of from the file handle. Actually, when compiling for
    /// the web via Emscripten, it forces in-memory loading regardless of the value passed to `Rstream::openFile`.
    /// For insound functionality, use this as a type of ifstream, instead of using any file object directly.
    /// You may also open an Rstreamable directly via `Rstreamable::create`, but this class provides an RAII container
    /// and interface to automatically handle cleanup and Rstreamable instantiation for you.
    ///
    /// @see `Rstreamable` and its various sub-classes for specific implementation details.
    class Rstream {
    public:
        Rstream() : m_stream() { }
        ~Rstream();

        // Non-copiable
        Rstream(const Rstream &) = delete;
        Rstream &operator=(const Rstream &) = delete;

        // Move constructor and assignment
        Rstream(Rstream &&other);
        Rstream &operator=(Rstream &&other);

        /// Open a file to begin streaming.
        /// @param filepath path to the file to open
        /// @param inMemory whether to load all file data in memory. On true, data is loaded into memory and
        ///        streamed from that memory; on false, data is streamed directly from file. (default: false)
        /// @note for now, only Emscripten enables access to http/https URL via JS fetch API.
        /// @note on Android, relative paths resolve to APK access, where absolute paths will traverse the
        ///       file system.
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

        /// Check whether the stream is open for reading
        [[nodiscard]]
        bool isOpen() const;

        /// Close the stream. Safe to call if the stream is already closed.
        void close();

        /// Move the read position within the stream. (Position is relative to the stream's beginning point, not the
        /// current read position. Use `tell` to get the current position, if you need relative positioning.)
        bool seek(int64_t position);

        /// Size of the stream in bytes. Returns `-1`, if there is no explicit length.
        [[nodiscard]]
        int64_t size() const;

        /// Get the current read position in bytes relative from the stream's beginning point.
        [[nodiscard]]
        int64_t tell() const;

        /// Whether the stream's eof flag was set. Rstream's eof behaves as an std::ifstream does, where it is raised
        /// only when a read past the end of the file is made.
        [[nodiscard]]
        bool isEof() const;

        /// Read a number of bytes from the stream into a target buffer.
        /// @returns the number of bytes read. `0` on eof, `-1` on error.
        /// Any number less than `byteCount` suggests that the end was reached.
        int64_t read(uint8_t *buffer, int64_t byteCount);

        /// Get the raw underlying Rstreamable object.
        class Rstreamable *stream() { return m_stream; }

    private:
        class Rstreamable *m_stream;
    };


}
