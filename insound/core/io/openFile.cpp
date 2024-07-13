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
        INSOUND_PUSH_ERROR(Result::RuntimeErr, "exception thrown while getting/closing fetch data");
        emscripten_fetch_close(fetch);
        return false;
    }
}

#endif


static constexpr int CHUNK_SIZE = 1024;

/// @param path path to file to open
/// @param allocCallback callback that allocates userdata
/// @param getBufCallback callback that returns pointer to contiguous data to write to for current byte offset
/// @param userdata to act on
/// @returns size of data retrieved, or 0 on error
static size_t openFileImpl(const std::string &path,
    void(*allocCallback)(void *, size_t),
    void *(*getBufCallback)(void *, size_t),
    void *userdata)
{
#ifdef __EMSCRIPTEN__
    if (!emscripten_is_main_browser_thread())
    {
        const std::string_view pathView(path);
        if ( (pathView.size() > 6 && pathView.substr(0, 6) == "https:") ||
             (pathView.size() > 5 && pathView.substr(0, 5) == "http:") )
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
        return 0;
    }

    const auto dataSize = file.size();

    if (dataSize == 0)
    {
        // since this function is intended for read-only ops, this is considered an error
        INSOUND_PUSH_ERROR(Result::FileOpenErr, "opened file is empty");
        return false;
    }

    allocCallback(userdata, dataSize);

    uint8_t buffer[CHUNK_SIZE];
    int64_t i = 0;
    for (; i <= dataSize - CHUNK_SIZE; i += CHUNK_SIZE) // read file in chunks
    {
        auto data = getBufCallback(userdata, i);
        if (file.read((uint8_t *)data, CHUNK_SIZE) <= 0)
        {
            // failure during read
            return 0;
        }
    }

    // Get leftovers
    if (i < dataSize)
    {
        auto data = getBufCallback(userdata, i);
        if (file.read((uint8_t *)data, dataSize - i) <= 0)
        {
            // failure during read
            return 0;
        }
    }

    return static_cast<size_t>(dataSize);
}

static inline void alloc_string_callback(void *userdata, size_t byteSize)
{
    auto str = static_cast<std::string *>(userdata);
    str->resize(byteSize, 0);
}

/// @returns pointer to memory to write to
static inline void *get_string_callback(void *userdata, size_t byteOffset)
{
    auto str = static_cast<std::string *>(userdata);
    return str->data() + byteOffset;
}

bool openFile(const std::string &path, std::string *outData)
{
    std::string str;
    const auto size = openFileImpl(path, alloc_string_callback, get_string_callback, &str);
    if (size == 0)
        return false;

    if (outData)
        outData->swap(str);

    return true;
}

static inline void alloc_buffer_callback(void *userdata, size_t byteSize)
{
    auto buf = static_cast<uint8_t **>(userdata);
    *buf = static_cast<uint8_t *>(std::malloc(byteSize));
}

static inline void *get_buffer_callback(void *userdata, size_t byteOffset)
{
    auto buf = static_cast<uint8_t **>(userdata);
    return (*buf) + byteOffset;
}

bool insound::openFile(const std::string &path, uint8_t **outData, size_t *outSize)
{
    uint8_t *data = nullptr;
    const auto size = openFileImpl(path, alloc_buffer_callback, get_buffer_callback, &data);
    if (size == 0)
        return false;

    if (outData)
        *outData = data;
    else
        std::free(data);

    if (outSize)
        *outSize = size;

    return true;
}
