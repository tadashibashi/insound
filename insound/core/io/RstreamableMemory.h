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

        bool open(const std::string &filepath) override;

        /// Open RstreamableMemory using an in-memory data pointer.
        /// Please make sure that this data pointer is valid during the
        /// lifetime that RstreamableMemory is open and reading from it.
        bool open(const uint8_t *data, size_t size);

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
        bool m_ownsData{};

        int64_t m_cursor{};
        bool m_eof{};
    };

}

