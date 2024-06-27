#pragma once
#ifdef __ANDROID__
class AAsset;

namespace insound {
    AAsset *openAsset(const char *filename); ///< Intended for single read into a buffer
    AAsset *openAssetStream(const char *filename); ///< Intended for streaming multiple reads
}
#endif
