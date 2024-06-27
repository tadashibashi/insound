#pragma once

#ifdef __ANDROID__
#include "Rstreamable.h"

class AAsset;

namespace insound {

    class RstreamableAAsset : public Rstreamable {
    public:
        RstreamableAAsset();
        ~RstreamableAAsset() override;

        bool open(const fs::path &filepath) override;
        [[nodiscard]]
        bool isOpen() const override;
        void close() override;

        bool seek(int64_t position) override;

        /// Return -1 if stream is infinite.
        [[nodiscard]]
        int64_t size() const override;
        [[nodiscard]]
        int64_t position() const override;

        /// If return value is not the number of bytes, and isEof is not true, then
        /// an error occurred.
        int64_t read(uint8_t *buffer, int64_t byteCount) override;
        [[nodiscard]]
        bool isEof() const override;

    private:
        AAsset *m_asset;
        int64_t m_pos, m_size;
    };

} // insound

#endif
