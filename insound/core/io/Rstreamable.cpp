#include "../io/Rstreamable.h"

#include "RstreamableAAsset.h"
#include "RstreamableFile.h"
#include "RstreamableMemory.h"
#include "../path.h"

insound::Rstreamable *insound::Rstreamable::create(const std::string &filepath, bool inMemory)
{
#if INSOUND_PLATFORM_EMSCRIPTEN
    inMemory = true; // force load into memory
#endif

    Rstreamable *stream;
    if (inMemory)
    {
        stream = new RstreamableMemory();
    }
    else
    {
#if INSOUND_PLATFORM_ANDROID

        if (path::isAbsolute(filepath))
        {
            stream = new RstreamableFile();
        }
        else
        {
            stream = new RstreamableAAsset();
        }
#else
        stream = new RstreamableFile();
#endif
    }

    if (!stream->open(filepath))
    {
        delete stream;
        return nullptr;
    }

    return stream;
}
