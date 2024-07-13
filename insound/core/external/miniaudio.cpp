#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_NO_DEVICE_IO
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_THREADING
#define MA_NO_ENGINE
#define MA_NO_GENERATION
#define MA_NO_SPATIAL

#ifndef INSOUND_DECODE_WAV
#define MA_NO_WAV
#endif

#ifndef INSOUND_DECODE_FLAC
#define MA_NO_FLAC
#endif

#ifndef INSOUND_DECODE_MP3
#define MA_NO_MP3
#endif

#define MA_NO_VORBIS // we'll use the libvorbis backend implementation
#define MA_NO_OPUS

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#ifdef MA_NO_LIBVORBIS
#undef MA_NO_LIBVORBIS
#endif

#include <insound/core/Error.h>

#include "miniaudio_libvorbis.h"
#include "miniaudio_ext.h"

#include <insound/core/io/Rstream.h>
#include <insound/core/io/Rstreamable.h>

#include <map>

static size_t dr_wav_read_callback(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    const auto streamable = static_cast<insound::Rstreamable *>(pUserData);
    return static_cast<size_t>(
        streamable->read(
            static_cast<uint8_t *>(pBufferOut),
            static_cast<int64_t>(bytesToRead)));
}

static ma_bool32 dr_wav_seek_callback(void* pUserData, int offset, ma_dr_wav_seek_origin origin)
{
    const auto streamable = static_cast<insound::Rstreamable *>(pUserData);
    if (origin == ma_dr_wav_seek_origin_current)
    {
        offset += static_cast<int>(streamable->tell());
    }

    return streamable->seek(offset);
}

bool insound_ma_dr_wav_get_markers(const std::string &filepath, std::vector<insound::Marker> *outMarkers)
{
    insound::Rstream stream;
    if (!stream.openFile(filepath))
    {
        INSOUND_PUSH_ERROR(insound::Result::FileOpenErr,
            "cannot get marker data because the file failed to open");
        return false;
    }

    ma_dr_wav wav;
    if (!ma_dr_wav_init_with_metadata(
        &wav,
        dr_wav_read_callback,
        dr_wav_seek_callback,
        stream.stream(),
        0,
        nullptr))
    {
        INSOUND_PUSH_ERROR(insound::Result::MaErr,
            "failed to init dr_wav with metadata");
        return false;
    }

    // Collect marker metadata
    std::map<uint32_t, insound::Marker> markers;
    try
    {
        const auto bytesPerFrame = wav.channels * (wav.bitsPerSample / CHAR_BIT);


        for (auto meta = wav.pMetadata, metaEnd = wav.pMetadata + wav.metadataCount;
            meta != metaEnd;
            ++meta)
        {
            switch(meta->type)
            {
                case ma_dr_wav_metadata_type_cue:
                {
                    const auto count = meta->data.cue.cuePointCount;
                    for (auto cue = meta->data.cue.pCuePoints, cueEnd = cue + count;
                        cue != cueEnd; ++cue)
                    {
                        markers[cue->id].position = cue->sampleByteOffset / bytesPerFrame;
                    }
                } break;

                case ma_dr_wav_metadata_type_list_label:
                {
                    const auto &[cuePointId, stringLength, pString] = meta->data.labelOrNote;
                    markers[cuePointId].label = std::string(pString, stringLength);
                } break;

                default:
                    break;
            }
        }
    }
    catch(const std::exception &e)
    {
        INSOUND_PUSH_ERROR(insound::Result::StdExcept, e.what());
        ma_dr_wav_uninit(&wav);
        throw;
    }
    catch(...)
    {
        INSOUND_PUSH_ERROR(insound::Result::UnknownErr,
            "an unknown exception was thrown while parsing WAV marker data");
        ma_dr_wav_uninit(&wav);
        throw;
    }

    ma_dr_wav_uninit(&wav);

    // Return markers
    if (outMarkers)
    {
        outMarkers->clear();
        outMarkers->reserve(markers.size());
        for (auto &[id, marker] : markers)
        {
            outMarkers->emplace_back(marker);
        }
    }

    return true;
}

