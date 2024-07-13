#pragma once
#include "Rstreamable.h"

#include <cstdint>
#include <string>

namespace insound {
    /// Opens a file into memory and enables streaming from that memory.
    /// Handles memory clean up in RAII fashion.
    class RstreamableMemory final : public Rstreamable {
    public:
        RstreamableMemory() = default;
        ~RstreamableMemory() override = default;

        /// Open data from a file and load it into memory, from which to stream
        /// @param filepath path to the file to open
        /// @returns whether function succeeded. Check `popError()` for more details.
        bool openFile(const std::string &filepath) override;

        /// Open RstreamableMemory using an in-memory data pointer.
        /// Please make sure that this data pointer is valid during the
        /// lifetime that RstreamableMemory is open and reading from it.
        /// @param data pointer containing data to stream
        /// @param size length of data in the buffer in bytes
        /// @returns whether function succeeded. Check `popError()` for more details.
        bool openConstMem(const uint8_t *data, size_t size);
        /// Open RstreamableMemory using an in-memory data pointer.
        /// This pointer is handed off to RstreamableMemory to clean up.
        /// @param data         data pointer to pass
        /// @param size         data buffer size in bytes
        /// @param deallocator  custom clean up function called on `close`, deafult calls
        ///                     std::free on pointer
        /// @returns whether function succeeded. Check `popError()` for more details.
        bool openMem(uint8_t *data, size_t size, void(*deallocator)(void *data) = nullptr);

        [[nodiscard]]
        bool isOpen() const override;
        void close() override;

        bool seek(int64_t position) override;

        [[nodiscard]]
        int64_t size() const override;
        [[nodiscard]]
        int64_t tell() const override;

        int64_t read(uint8_t *buffer, int64_t requestedBytes) override;

        [[nodiscard]]
        bool isEof() const override;
    private:
        uint8_t *m_data{};
        size_t m_size{};
        void(*m_deallocator)(void *){};
        int64_t m_cursor{};
        bool m_eof{};
    };

}

