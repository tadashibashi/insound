#include "loadAudio.h"

#include "decodeFLAC.h"
#include "decodeGME.h"
#include "decodeMp3.h"
#include "decodeVorbis.h"
#include "decodeWAV.h"

#include "../AudioSpec.h"
#include "../Error.h"
#include "../Marker.h"
#include "../io/openFile.h"

bool insound::loadAudio(const fs::path &path, const AudioSpec &targetSpec, uint8_t **outBuffer, uint32_t *outLength, std::map<uint32_t, Marker> *outMarkers)
{
    // Detect audio file type by extension
    if (!path.has_extension())
    {
        pushError(Result::InvalidArg,
                  R"(`path` must contain a recognizable extension e.g. ".wav", ".ogg")");
        return false;
    }

    // Uppercase extension
    std::string ext = path.extension().string();
    for (auto &c : ext)
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
    std::map<uint32_t, Marker> markers;

    const auto fileDataBuf = (uint8_t *)fileData.data();
    const auto fileDataSize = static_cast<uint32_t>(fileData.size());

    if (ext == ".WAV")
    {
        if (decodeWAV_v2(fileDataBuf, fileDataSize, &spec, &buffer, &bufferSize, &markers) != WAVResult::Ok)
        {
            // fallback to SDL's WAV parser
            if (!decodeWAV((uint8_t *)fileData.data(), fileDataSize, &spec, &buffer, &bufferSize))
                return false;
        }
    }
    else if (ext == ".OGG")
    {
#ifdef INSOUND_DECODE_VORBIS
        if (!decodeVorbis(fileDataBuf, fileDataSize, &spec, &buffer, &bufferSize))
            return false;
#else
        pushError(Result::NotSupported, "Vorbis decoding is not supported, make sure to compile with "
            "INSOUND_DECODE_VORBIS defined");
#endif
    }
    else if (ext == ".FLAC")
    {
#ifdef INSOUND_DECODE_FLAC
        if (!decodeFLAC(fileDataBuf, fileDataSize, &spec, &buffer, &bufferSize))
            return false;
#else
        pushError(Result::NotSupported, "FLAC decoding is not supported, make sure to compile with "
            "INSOUND_DECODE_FLAC defined");
        return false;
#endif
    }
    else if (ext == ".MP3")
    {
#ifdef INSOUND_DECODE_MP3
        if (!decodeMp3(fileDataBuf, fileDataSize, &spec, &buffer, &bufferSize))
            return false;
#else
        pushError(Result::NotSupported, "MP3 decoding is not supported, make sure to compile with "
            "INSOUND_DECODE_MP3 defined");
        return false;
#endif
    }
    else
    {
#ifdef INSOUND_DECODE_GME
        if (!decodeGME(fileDataBuf, fileDataSize, targetSpec.freq, 0, -1, &spec, &buffer, &bufferSize))
            return false;
#else
        pushError(Result::NotSupported, "GME decoding is not supported, make sure to compile with "
            "INSOUND_DECODE_GME defined");
        return false;
#endif
    }

    uint32_t newSize;

    // Convert audio format to the target format.
    // This is because insound's audio engine depends on the loaded sound buffer formats being the same as its engine's.
    if (!convertAudio(buffer, bufferSize, spec, targetSpec, &buffer, &newSize))
    {
        std::free(buffer);
        return false;
    }

    // Adjust marker positions to new samplerate
    if (newSize != bufferSize)
    {
        const auto sizeFactor = (float)newSize / (float)bufferSize;
        for (auto &[id, marker] : markers)
        {
            marker.position = (uint32_t)std::round((float)marker.position * sizeFactor);
        }
    }

    // Done, return the out values
    if (outBuffer)
        *outBuffer = buffer;
    else // if outBuffer is discarded, free the buffer
        std::free(buffer);

    if (outLength)
        *outLength = newSize;

    if (outMarkers)
        outMarkers->swap(markers);

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
        pushError(Result::SdlErr, SDL_GetError());

    return cvtResult == 0;
}
