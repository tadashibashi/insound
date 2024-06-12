#include "openFile.h"
#include "../Error.h"

#include <fstream>

namespace insound {
    bool openFile(const std::filesystem::path &path, std::string *outData)
    {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
            pushError(Result::FileOpenErr, std::strerror(errno));
            return false;
        }

        if (!file.seekg(0, std::ios_base::end))
        {
            pushError(Result::RuntimeErr, "openFile: file failed to seek");
            return false;
        }

        std::string data;
        const auto fileSize = file.tellg();

        try
        {
            data.resize((size_t)fileSize, 0);
        }
        catch (const std::length_error &e)
        {
            pushError(Result::RuntimeErr, "openFile: File size exceeded max_size()");
            return false;
        }
        catch(...)
        {
            pushError(Result::RuntimeErr, "openFile: Failed occurred while resizing data buffer");
            return false;
        }

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

        if (outData)
        {
            outData->swap(data);
        }

        return true;
    }
}
