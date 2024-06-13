#include "loadAudio.h"

#include "../AudioSpec.h"
#include "../Error.h"
#include "decodeFLAC.inl"
#include "decodeGME.inl"
#include "decodeMp3.inl"
#include "decodeVorbis.inl"
#include "decodeWAV.inl"

#include <fstream>
#include <iostream>

bool insound::loadAudio(const std::filesystem::path &path, const AudioSpec &targetSpec, uint8_t **outBuffer, uint32_t *outLength)
{
    if (!path.has_extension())
    {
        pushError(Result::InvalidArg, R"(`path` must contain a recognizable extension e.g. ".wav", ".ogg")");
        return false;
    }

    // Make extension uppercase
    auto ext = path.extension().string();
    for (char &c : ext)
    {
        c = (char)std::toupper(c);
    }

    AudioSpec spec;
    uint8_t *buffer;
    uint32_t bufferSize;

    if (ext == ".WAV")
    {
        if (!decodeWAV(path, &spec, &buffer, &bufferSize))
        {
            return false;
        }
    }
    else if (ext == ".OGG")
    {
        if (!decodeVorbis(path, &spec, &buffer, &bufferSize))
        {
            return false;
        }
    }
    else if (ext == ".FLAC")
    {
        if (!decodeFLAC(path, &spec, &buffer, &bufferSize))
        {
            return false;
        }
    }
    else if (ext == ".MP3")
    {
        if (!decodeMp3(path, &spec, &buffer, &bufferSize))
        {
            return false;
        }
    }
    else
    {
        if (!decodeGME(path, targetSpec.freq, 0, -1, &spec, &buffer, &bufferSize))
        {
            return false;
        }
    }

    // convert audio
    if (!convertAudio(buffer, bufferSize, spec, targetSpec, &buffer, &bufferSize))
    {
        free(buffer);
        return false;
    }

    if (outBuffer)
    {
        *outBuffer = buffer;
    }
    else
    {
        free(buffer);
    }

    if (outLength)
    {
        *outLength = bufferSize;
    }

    return true;
}

bool insound::convertAudio(uint8_t *audioData, const uint32_t length, const AudioSpec &dataSpec,
                           const AudioSpec &targetSpec, uint8_t **outBuffer, uint32_t *outLength)
{
    SDL_AudioCVT cvt;
    auto cvtResult = SDL_BuildAudioCVT(&cvt,
        dataSpec.format.flags(), dataSpec.channels, dataSpec.freq,
        targetSpec.format.flags(), targetSpec.channels, targetSpec.freq);

    if (cvtResult < 0)
    {
        pushError(Result::SdlErr, SDL_GetError());
        return false;
    }

    if (cvtResult == 0 && !cvt.needed) // no conversion needed, same formats
    {
        if (outLength)
            *outLength = length;
        if (outBuffer)
            *outBuffer = audioData;
        else
            free(audioData);
        return true;
    }

    // Convert audio
    cvt.len = (int)length;
    cvt.buf = (uint8_t *)realloc(audioData, cvt.len * cvt.len_mult);
    cvtResult = SDL_ConvertAudio(&cvt);

    if (outLength)
        *outLength = cvt.len_cvt;

    if (outBuffer)
        *outBuffer = cvt.buf;
    else
        free(cvt.buf);

    if (cvtResult != 0)
        pushError(Result::SdlErr, SDL_GetError());

    return cvtResult == 0;
}
