#pragma once

#include <cstdint>
#include <filesystem>

namespace insound {
    namespace fs = std::filesystem;

    /// Low level resource stream: an abstraction around read-only file streams
    class Rstreamable {
    public:
        virtual ~Rstreamable() = default;

        virtual bool open(const fs::path &filepath) = 0;

        [[nodiscard]]
        virtual bool isOpen() const = 0;

        virtual void close() = 0;

        virtual bool seek(int64_t position) = 0;

        /// Return -1 if stream is infinite.
        [[nodiscard]]
        virtual int64_t size() const = 0;

        [[nodiscard]]
        virtual int64_t position() const = 0;

        /// If return value is -1 an error occurred, otherwise it will return the number of
        /// bytes read.
        virtual int64_t read(uint8_t *buffer, int64_t bytes) = 0;

        [[nodiscard]]
        virtual bool isEof() const = 0;
    };
}
