#include "Rstreamable.h"

#include "RstreamableAAsset.h"
#include "RstreamableFile.h"


insound::Rstreamable *insound::Rstreamable::create(const fs::path &filepath)
{
    Rstreamable *stream = nullptr;
#ifdef __ANDROID__

    if (filepath.is_absolute())
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
