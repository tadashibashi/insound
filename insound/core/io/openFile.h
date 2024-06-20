#pragma once
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace insound {


    /// Open a file and populate a buffer with its contents. Blocking function.
    /// @param path    path to the file to open. TODO: support emscripten_fetch, and curl on non-emscripten platforms
    /// @param outData string buffer to receive the data. We're using string because it automatically appends a
    ///                null terminator in case text should be used like a c-string
    bool openFile(const fs::path &path, std::string *outData);
}
