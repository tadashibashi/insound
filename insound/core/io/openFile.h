#pragma once
#include <string>

namespace insound {

    /// Open a file and populate a buffer with its contents. Blocking function.
    /// On Emscripten platform, http/https URLs are fetched.
    /// @param path    path to the file to open. TODO: support curl on non-emscripten platforms
    /// @param outData string buffer to receive the data. We're using string because it automatically appends a
    ///                null terminator in case text should be used like a c-string
    bool openFile(const std::string &path, std::string *outData);

    /// Open a file and receive a buffer with its contents. Blocking function.
    /// On Emscripten platform, http/https URLS are fetched.
    /// @param path     path to the file to open.
    /// @param outData  pointer to receive the data buffer. Make sure to call `std::free` on it when data is
    ///                 no longer needed.
    /// @param outSize  pointer to receive the buffer size in bytes.
    /// @returns whether function succeeded. Out variables will remain unmutated on false, and will
    ///          be filled on true.
    bool openFile(const std::string &path, uint8_t **outData, size_t *outSize);
}
