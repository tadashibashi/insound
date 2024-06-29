#pragma once

#include <cstdint>
#include <filesystem>

namespace insound {
    namespace fs = std::filesystem;

    class Rstream {
    public:
        Rstream() : m_stream() { }
        ~Rstream();
        bool open(const fs::path &filepath);

        [[nodiscard]]
        bool isOpen() const;
        void close();

        bool seek(int64_t position);

        [[nodiscard]]
        int64_t size() const;

        [[nodiscard]]
        int64_t position() const;

        [[nodiscard]]
        bool isEof() const;

        int64_t read(uint8_t *buffer, int64_t byteCount);

        class Rstreamable *stream() { return m_stream; }

    private:
        class Rstreamable *m_stream;
    };


}
