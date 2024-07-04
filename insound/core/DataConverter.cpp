#include "DataConverter.h"

using namespace insound;

struct DataConverter::Impl {
    AudioSpec inSpec, outSpec;
};

bool DataConverter::convert(uint8_t **inFrames, uint64_t inFrameCount) {
#if defined(INSOUND_BACKEND_SDL2)
    SDL_AudioCVT cvt;
    auto cvtResult = SDL_BuildAudioCVT(&cvt,
        inSpec.format.flags(), inSpec.channels, inSpec.freq,
        outSpec.format.flags(), outSpec.channels, outSpec.freq);

    if (cvtResult < 0)
    {
        INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
        return false;
    }

    if (cvtResult == 0 && !cvt.needed) // no conversion needed, same formats
    {
        if (outLength)
            *outLength = length;
        if (outBuffer)
            *outBuffer = audioData;
        else
            std::free(audioData);
        return true;
    }

    // Convert audio
    cvt.len = (int)length;
    cvt.buf = (uint8_t *)std::realloc(audioData, cvt.len * cvt.len_mult);
    cvtResult = SDL_ConvertAudio(&cvt);

    if (outLength)
        *outLength = cvt.len_cvt;

    if (outBuffer)
        *outBuffer = cvt.buf;
    else
        std::free(cvt.buf);

    if (cvtResult != 0)
        INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());

    return cvtResult == 0;;
#elif defined(INSOUND_BACKEND_SDL3)
    return false; // TODO: implement
#else
    return false; // TODO: implement
#endif
}
