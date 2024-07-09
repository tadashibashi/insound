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
        std::string m_data{};
        int64_t m_cursor{};
        bool m_eof{};
    };

}

