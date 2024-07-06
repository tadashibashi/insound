#include "../io/Rstreamable.h"

#include "RstreamableAAsset.h"
#include "RstreamableFile.h"
#include "../path.h"

insound::Rstreamable *insound::Rstreamable::create(const std::string &filepath)
{
    Rstreamable *stream = nullptr;
#ifdef __ANDROID__

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

    if (!stream) return nullptr;

    if (!stream->open(filepath))
    {
        delete stream;
        return nullptr;
    }

    return stream;
}
