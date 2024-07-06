#include "openFile.h"
#include "../Error.h"
#include "Rstream.h"

#include <cstring>
#include <fstream>

using namespace insound;

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#include <emscripten/threading.h>

static bool openFileURLSync(const std::string &url, std::string *outData)
{
    if (!outData)
    {
        INSOUND_PUSH_ERROR(insound::Result::InvalidArg, "openFileURLSync: outData must not be NULL");
        return false;
    }

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
    auto fetch = emscripten_fetch(&attr, url.c_str());
    if (!fetch)
    {
        INSOUND_PUSH_ERROR(insound::Result::LogicErr, "openFileURLSync may have been called from the main thread");
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

        INSOUND_PUSH_ERROR(insound::Result::RuntimeErr, "Http request received a non-200 status code");
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

static bool openFileFstream(const std::string &path, std::string *outData)
{
    // Open the file
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        INSOUND_PUSH_ERROR(Result::FileOpenErr, std::strerror(errno));
        return false;
    }

    // Seek to the end of the file to get the file size
    if (!file.seekg(0, std::ios_base::end))
    {
        INSOUND_PUSH_ERROR(Result::RuntimeErr, "openFile: file failed to seek");
        return false;
    }

    std::string data;
    const auto fileSize = file.tellg();

    // Prepare the string buffer to fill
    try {
        data.resize((size_t)fileSize, 0);
    }
    catch (const std::length_error &e) {
        INSOUND_PUSH_ERROR(Result::RuntimeErr, "openFile: file size exceeded max_size()");
        return false;
    }
    catch(...) {
        INSOUND_PUSH_ERROR(Result::RuntimeErr, "openFile: failed occurred while resizing data buffer");
        return false;
    }

    // Seek back to the beginning, and perform the read
    if (!file.seekg(0, std::ios_base::beg))
    {
        INSOUND_PUSH_ERROR(Result::RuntimeErr, "openFile: file failed to seek");
        return false;
    }

    if (!file.read(data.data(), fileSize))
    {
        INSOUND_PUSH_ERROR(Result::RuntimeErr, "openFile: failed to read file");
        return false;
    }

    // Return the out value
    if (outData)
    {
        outData->swap(data);
    }

    return true;
}

static constexpr int CHUNK_SIZE = 1024;

bool insound::openFile(const std::string &path, std::string *outData)
{
#ifdef __EMSCRIPTEN__
    if (!emscripten_is_main_browser_thread())
    {
        const std::string_view pathView(path);
        if (pathView.size() > 4 && pathView.substr(0, 4) == "http")
        {
            return openFileURLSync(path, outData);
        }
    }
#else
    // todo: curl implementation
#endif

    Rstream file;
    if (!file.open(path))
    {
        return false;
    }

    const auto dataSize = file.size();

    std::string data;
    data.resize(dataSize, 0);

    int64_t i = 0;
    for (; i <= dataSize - CHUNK_SIZE; i += CHUNK_SIZE) // read file in chunks
    {
        if (file.read((uint8_t *)data.data() + i, CHUNK_SIZE) <= 0)
        {
            // failure during read
            return false;
        }
    }

    // Get leftovers
    if (i < dataSize)
    {
        if (file.read((uint8_t *)data.data() + i, dataSize - i) <= 0)
        {
            // failure during read
            return false;
        }
    }

    if (outData)
    {
        outData->swap(data);
    }

    return true;
}
