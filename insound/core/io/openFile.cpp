#include "openFile.h"
#include "../Error.h"

#include <cstring>
#include <fstream>
#include <iostream>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#include <emscripten/threading.h>

static bool openFileURLSync(const fs::path &url, std::string *outData)
{
    if (!outData)
    {
        insound::pushError(insound::Result::InvalidArg, "openFileURLSync: outData must not be NULL");
        return false;
    }

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
    auto fetch = emscripten_fetch(&attr, url.c_str());
    if (!fetch)
    {
        insound::pushError(insound::Result::LogicErr, "openFileURLSync may have been called from the main thread");
        return false;
    }

    try
    {
        if (fetch->status == 200)
        {
            outData->resize(fetch->numBytes, 0);
            std::memcpy(outData->data(), fetch->data, fetch->numBytes);

            emscripten_fetch_close(fetch);
            return true;
        }

        // TODO: handle redirects, or check if emscripten_fetch handles it for us.

        insound::pushError(insound::Result::RuntimeErr, "Http request received a non-200 status code");
        emscripten_fetch_close(fetch);
        return false;
    }
    catch(...)
    {
        std::cout << "Failed in catch block\n";
        emscripten_fetch_close(fetch);
        return false;
    }
}

#endif

bool insound::openFile(const fs::path &path, std::string *outData)
{
#ifdef __EMSCRIPTEN__
    if (!emscripten_is_main_browser_thread())
    {
        const std::string_view pathView(path.native());
        if (pathView.size() > 4 && pathView.substr(0, 4) == "http")
        {
            return openFileURLSync(path, outData);
        }
    }
#else
    // todo: curl implementation
#endif
    // Open the file
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        pushError(Result::FileOpenErr, std::strerror(errno));
        return false;
    }

    // Seek to the end of the file to get the file size
    if (!file.seekg(0, std::ios_base::end))
    {
        pushError(Result::RuntimeErr, "openFile: file failed to seek");
        return false;
    }

    std::string data;
    const auto fileSize = file.tellg();

    // Prepare the string buffer to fill
    try {
        data.resize((size_t)fileSize, 0);
    }
    catch (const std::length_error &e) {
        pushError(Result::RuntimeErr, "openFile: file size exceeded max_size()");
        return false;
    }
    catch(...) {
        pushError(Result::RuntimeErr, "openFile: failed occurred while resizing data buffer");
        return false;
    }

    // Seek back to the beginning, and perform the read
    if (!file.seekg(0, std::ios_base::beg))
    {
        pushError(Result::RuntimeErr, "openFile: file failed to seek");
        return false;
    }

    if (!file.read(data.data(), fileSize))
    {
        pushError(Result::RuntimeErr, "openFile: failed to read file");
        return false;
    }

    // Return the out value
    if (outData)
    {
        outData->swap(data);
    }

    return true;
}
