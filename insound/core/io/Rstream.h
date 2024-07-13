#pragma once

#include <cstdint>
#include <string>

namespace insound {

    class Rstream {
    public:
        Rstream() : m_stream() { }
        Rstream(Rstream &&other);
        ~Rstream();

        Rstream &operator=(Rstream &&other);

        bool open(const std::string &filepath, bool inMemory = false);
        bool openConstMem(const uint8_t *data, size_t size);

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
