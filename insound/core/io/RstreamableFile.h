#pragma once
#include "Rstreamable.h"

#include <fstream>

namespace insound {
    class RstreamableFile final : public Rstreamable {
    public:
        RstreamableFile();
        ~RstreamableFile() override;

        bool open(const std::string &filepath) override;
        [[nodiscard]]
        bool isOpen() const override;
        void close() override;

        bool seek(int64_t position) override;

        /// Return -1 if stream is infinite.
        [[nodiscard]]
        int64_t size() const override;
        [[nodiscard]]
        int64_t tell() const override;

        /// If return value is not the number of bytes, and isEof is not true, then
        /// an error occurred.
        int64_t read(uint8_t *buffer, int64_t bytes) override;

        [[nodiscard]]
        bool isEof() const override;
    private:
        mutable std::ifstream m_stream;
    };

}
