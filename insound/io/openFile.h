#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

namespace insound {
    bool openFile(const std::filesystem::path &path, std::string *outData);
}
