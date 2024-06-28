#include "RstreamableAAsset.h"

#ifdef __ANDROID__
#include "../platform/AndroidNative.h"
#include "../Error.h"

#include <android/asset_manager.h>

namespace insound {
    RstreamableAAsset::RstreamableAAsset() : m_asset(), m_pos(), m_size()
    {}

    RstreamableAAsset::~RstreamableAAsset()
    {
        if (m_asset)
        {
            AAsset_close(m_asset);
        }
    }

    bool RstreamableAAsset::open(const fs::path &filepath)
    {
        auto asset = openAssetStream(filepath.c_str());
        if (!asset) // TODO: forward the error?
            return false;

        // Clean up existing asset
        if (m_asset)
        {
            AAsset_close(m_asset);
        }

        // Commit result
        m_asset = asset;
        m_pos = 0;
        m_size = AAsset_getLength64(asset);
        return true;
    }

    void RstreamableAAsset::close()
    {
        if (m_asset)
        {
            AAsset_close(m_asset);
            m_asset = nullptr;
            m_size = 0;
        }
    }

    bool RstreamableAAsset::isOpen() const
    {
        return m_asset != nullptr;
    }

    bool RstreamableAAsset::seek(int64_t position)
    {
        position = std::min(position, m_size);
        const auto result = AAsset_seek64(m_asset, static_cast<off64_t>(position), SEEK_SET);
        if (result == -1)
        {
            INSOUND_PUSH_ERROR(Result::RuntimeErr, "Failure during AAsset_seek64");
            return false;
        }

        m_pos = position;
        return true;
    }

    int64_t RstreamableAAsset::size() const
    {
        return m_size;
    }

    int64_t RstreamableAAsset::position() const
    {
        return m_pos;
    }

    int64_t RstreamableAAsset::read(uint8_t *buffer, int64_t byteCount)
    {
        const auto count = AAsset_read(m_asset, buffer, byteCount);
        m_pos += count;
        return count;
    }

    bool RstreamableAAsset::isEof() const
    {
        return m_pos >= m_size;
    }


} // insound
#endif
