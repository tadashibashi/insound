#pragma once
#include <string>
#include <string_view>

namespace insound::path {
    std::string join(std::string_view a, std::string_view b);
    [[nodiscard]] std::string_view trim(std::string_view path);
    [[nodiscard]] bool isAbsolute(std::string_view path);
    [[nodiscard]] inline bool isRelative(const std::string_view path) { return !isAbsolute(path); }
    [[nodiscard]] bool hasExtension(std::string_view path);

    /// Get the extension portion of a path string.
    /// Returns a zero-length string_view if there is none
    std::string_view extension(std::string_view path);
}
