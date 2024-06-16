#include "loadAudio.h"

#include "decodeFLAC.h"
#include "decodeGME.h"
#include "decodeMp3.h"
#include "decodeVorbis.h"
#include "decodeWAV.h"

#include <insound/AudioSpec.h>
#include <insound/Error.h>
#include <insound/io/openFile.h>

bool insound::loadAudio(const std::filesystem::path &path, const AudioSpec &targetSpec, uint8_t **outBuffer, uint32_t *outLength)
{
    // Detect audio file type by extension
    if (!path.has_extension())
    {
        pushError(Result::InvalidArg,
                  R"(`path` must contain a recognizable extension e.g. ".wav", ".ogg")");
        return false;
    }

    // Uppercase extension
    auto ext = path.extension().native();
    for (char &c : ext)
    {
        c = (char)std::toupper(c);
    }

    // Load file data
    std::string fileData;
    if (!openFile(path, &fileData))
    {
        return false;
    }

    AudioSpec spec;
    uint8_t *buffer;
    uint32_t bufferSize;

    if (ext == ".WAV")
    {
        if (decodeWAV_v2((uint8_t *)fileData.data(), fileData.size(), &spec, &buffer, &bufferSize, nullptr) != WAVResult::Ok)
            return false;
    }
    else if (ext == ".OGG")
    {
        if (!decodeVorbis((uint8_t *)fileData.data(), fileData.size(), &spec, &buffer, &bufferSize))
            return false;
    }
    else if (ext == ".FLAC")
    {
        if (!decodeFLAC((uint8_t *)fileData.data(), fileData.size(), &spec, &buffer, &bufferSize))
            return false;
    }
    else if (ext == ".MP3")
    {
        if (!decodeMp3((uint8_t *)fileData.data(), fileData.size(), &spec, &buffer, &bufferSize))
            return false;
    }
    else
    {
        if (!decodeGME((uint8_t *)fileData.data(), fileData.size(), targetSpec.freq, 0, -1, &spec, &buffer, &bufferSize))
            return false;
    }

    // Convert audio format to the target format.
    // This is because insound's audio engine depends on the loaded sound buffer formats being the same as its engine's.
    if (!convertAudio(buffer, bufferSize, spec, targetSpec, &buffer, &bufferSize))
    {
        free(buffer);
        return false;
    }

    // Done, return the out values
    if (outBuffer)
        *outBuffer = buffer;
    else // if outBuffer is discarded, free the buffer
        free(buffer);

    if (outLength)
        *outLength = bufferSize;

    return true;
}

#include <SDL2/SDL_audio.h>

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
