#include "path.h"
#include "lib.h"

std::string insound::path::join(std::string_view a, std::string_view b)
{
    a = trim(a);
    b = trim(b);

    std::string res;
    auto aEnd = a.size();
    while (aEnd > 0 && a[aEnd-1] == '/')
        --aEnd;

    auto bStart = 0;
    while(bStart < b.size() && b[bStart] == '/')
        ++bStart;

    res.reserve(aEnd + b.size() - bStart);

    if (aEnd > 0)
        res += a.substr(0, aEnd);

    if (bStart < b.size())
    {
        res += "/";
        res += b.substr(bStart);
    }

    return res;
}

std::string_view insound::path::trim(std::string_view path)
{
    if (path.empty()) return "";

    size_t start = 0, end = path.size();
    while (start < end && std::isspace(path[start]))
        ++start;
    if (start == end) // all spaces
        return "";

    --end;
    while (end > start && std::isspace(path[end]))
        --end;

    return path.substr(start, end - start + 1);
}

bool insound::path::isAbsolute(std::string_view path)
{
    path = trim(path);

    if (path.empty())
        return false;

#if INSOUND_TARGET_WINDOWS
    return (path.size() > 1) && std::isalpha(path[0]) && path[1] == ':';
#else
    return path[0] == '/';
#endif
}

bool insound::path::hasExtension(std::string_view path)
{
    path = trim(path);

    if (path.empty())
        return false;

    // Find the dot index
    size_t dotIndex = path.size() - 1;
    while(dotIndex > 0 && path[dotIndex] != '.')
        --dotIndex;
    if (dotIndex == 0 || path[dotIndex-1] == '/' || path[dotIndex-1] == '\\') // a dot preceding the filename
        return false;
    return true;
}

std::string_view insound::path::extension(std::string_view path)
{
    path = trim(path);

    if (path.empty())
        return "";

    // Find the dot index
    size_t dotIndex = path.size() - 1;
    while(dotIndex > 0 && path[dotIndex] != '.')
        --dotIndex;
    if (dotIndex == 0 || path[dotIndex-1] == '/' || path[dotIndex-1] == '\\') // a dot preceding the filename
        return "";
    return path.substr(dotIndex);
}
