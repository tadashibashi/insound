#include "decodeGME.h"

#include <insound/Error.h>

#ifdef INSOUND_DECODE_GME
#include <gme/gme.h>

#include <climits>
#include <cstdlib>

bool insound::decodeGME(const uint8_t *memory, uint32_t size, int samplerate, int track, int lengthMS, AudioSpec *outSpec, uint8_t **outData,
                        uint32_t *outBufferSize)
{
    Music_Emu *emu;
    if (const auto result = gme_open_data(memory, size, &emu, samplerate); result != nullptr)
    {
        pushError(Result::GmeErr, result);
        return false;
    }

    if (lengthMS <= 0)
    {
        gme_info_t *info;
        if (const auto result = gme_track_info(emu, &info, track); result != nullptr)
        {
            pushError(Result::GmeErr, result);
            gme_delete(emu);
            return false; // todo: allow for fallback value?
        }

        if (info->length != -1)
        {
            lengthMS = info->length;
        }
        else
        {
            lengthMS = 120 * 1000; // 2 minute default
        }

        gme_free_info(info);
    }


    if (const auto result = gme_start_track(emu, track); result != nullptr)
    {
        pushError(Result::GmeErr, result);
        gme_delete(emu);
        return false;
    }

    uint32_t sampleSize = samplerate * (lengthMS / 1000.f) * 2;
    uint32_t bufSize = sampleSize * sizeof(short);

    short *buf = (short *)std::malloc(bufSize);

    uint32_t i = 0;
    for (; i < sampleSize - 1024; i += 1024)
    {
        if (const auto result = gme_play(emu, 1024, buf + i); result != nullptr)
        {
            pushError(Result::GmeErr, result);
            free(buf);
            gme_delete(emu);
            return false;
        }
    }

    // Get the rest
    if (const auto result = gme_play(emu, sampleSize - i, buf + i); result != nullptr)
    {
        pushError(Result::GmeErr, result);
        free(buf);
        gme_delete(emu);
        return false;
    }

    if (outSpec)
    {
        AudioSpec spec;
        spec.channels = 2;
        spec.freq = samplerate;
        spec.format = SampleFormat(sizeof(short) * CHAR_BIT, false, false, true);
        *outSpec = spec;
    }

    if (outData)
    {
        *outData = (uint8_t *)buf;
    }
    else
    {
        free(buf);
    }

    if (outBufferSize)
    {
        *outBufferSize = bufSize;
    }

    gme_delete(emu);
    return true;
}

#else
bool insound::decodeGME(const uint8_t *memory, uint32_t size, int samplerate, int track, int lengthMS, AudioSpec *outSpec, uint8_t **outData,
                        uint32_t *outBufferSize)
{
    pushError(Result::NotSupported, "libgme decoding is not supported, make sure to compile with "
        "INSOUND_DECODE_GME defined");
    return false;
}
#endif
