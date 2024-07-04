#include "decodeGME.h"

#ifdef INSOUND_DECODE_GME
#include "../Error.h"
#include "Rstream.h"
#include "Rstreamable.h"

#include <gme/gme.h>

#include <climits>
#include <cstdlib>

bool insound::decodeGME(const uint8_t *memory, uint32_t size, int samplerate, int track, int lengthMS, AudioSpec *outSpec, uint8_t **outData,
                        uint32_t *outBufferSize)
{
    Music_Emu *emu;
    if (const auto result = gme_open_data(memory, size, &emu, samplerate); result != nullptr)
    {
        INSOUND_PUSH_ERROR(Result::GmeErr, result);
        return false;
    }

    if (lengthMS <= 0)
    {
        gme_info_t *info;
        if (const auto result = gme_track_info(emu, &info, track); result != nullptr)
        {
            INSOUND_PUSH_ERROR(Result::GmeErr, result);
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
        INSOUND_PUSH_ERROR(Result::GmeErr, result);
        gme_delete(emu);
        return false;
    }

    uint32_t sampleSize = samplerate * (lengthMS / 1000.0) * 2.0;
    uint32_t bufSize = sampleSize * sizeof(short);

    short *buf = (short *)std::malloc(bufSize);

    uint32_t i = 0;
    for (; i <= sampleSize - 1024; i += 1024)
    {
        if (const auto result = gme_play(emu, 1024, buf + i); result != nullptr)
        {
            INSOUND_PUSH_ERROR(Result::GmeErr, result);
            free(buf);
            gme_delete(emu);
            return false;
        }
    }

    if (i < sampleSize)
    {
        // Get the rest
        if (const auto result = gme_play(emu, static_cast<int>(sampleSize) - i, buf + i); result != nullptr)
        {
            INSOUND_PUSH_ERROR(Result::GmeErr, result);
            free(buf);
            gme_delete(emu);
            return false;
        }
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
    INSOUND_PUSH_ERROR(Result::NotSupported, "libgme decoding is not supported, make sure to compile with "
        "INSOUND_DECODE_GME defined");
    return false;
}
#endif

namespace insound {
#ifdef INSOUND_DEBUG
#define INIT_GUARD() do { if (!isOpen()) { \
    INSOUND_PUSH_ERROR(Result::LogicErr, "GmeDecoder::Impl::emu was not init"); \
    return false; \
} } while(0)
#define GME_ERR_CHECK(statement) do { if (const auto result = (statement); result != nullptr) { \
        INSOUND_PUSH_ERROR(Result::GmeErr, result); \
        return false; \
    } } while(0)
#else
#define INIT_GUARD()
#define GME_ERR_CHECK(statement) (statement)
#endif

    static gme_err_t gme_read_rstream(void *userdata, void *out, const int count)
    {
        auto stream = static_cast<Rstreamable *>(userdata);
        if (const auto bytesRead = stream->read(static_cast<uint8_t *>(out), count);
            bytesRead < 0)
        {
            return "Rstream failed during read of GME file";
        }

        return nullptr;
    }

    struct GmeDecoder::Impl {
        Music_Emu *emu{};
        Rstream stream;
        int trackNumber;
    };

    GmeDecoder::GmeDecoder(GmeDecoder &&other) noexcept :
        AudioDecoder(std::move(other)), m(other.m)
    {
        other.m = nullptr;
    }

    GmeDecoder::GmeDecoder(const int samplerate) : m(new Impl)
    {
        m_spec.freq = samplerate;
    }

    GmeDecoder::~GmeDecoder()
    {
        if (m && m->emu)
            gme_delete(m->emu);

        delete m;
    }

    int GmeDecoder::read(const int sampleFrames, uint8_t *data)
    {
        const auto lastSamples = gme_tell_samples(m->emu);

        if (const auto result = gme_play(m->emu, sampleFrames * 2, (short *)data);
            result != nullptr)
        {
            INSOUND_PUSH_ERROR(Result::GmeErr, result); // todo: this could lead to a stack overflow
            return 0;
        }

        return (gme_tell_samples(m->emu) - lastSamples) / 2;
    }

    bool GmeDecoder::isEnded(bool *outEnded) const
    {
        if (outEnded)
            *outEnded = false;
        // TODO: support lengths, for now the track just plays until instructions stop
        return false;
    }

    bool GmeDecoder::getMaxFrames(uint64_t *outMaxFrames) const
    {
        if (outMaxFrames)
            *outMaxFrames = UINT64_MAX;
        return true;
    }

    bool GmeDecoder::getTrackCount(int *outTrackCount) const
    {
        INIT_GUARD();

        if (outTrackCount)
            *outTrackCount = gme_track_count(m->emu);
        return true;
    }

    bool GmeDecoder::isOpen() const
    {
        return m != nullptr && m->emu != nullptr;
    }

    bool GmeDecoder::setTrack(int track)
    {
        INIT_GUARD();

        GME_ERR_CHECK(gme_start_track(m->emu, track));
        m->trackNumber = track;
        return true;
    }

    bool GmeDecoder::getTrack(int *outTrack) const
    {
        INIT_GUARD();

        if (outTrack)
            *outTrack = m->trackNumber;
        return true;
    }

    bool GmeDecoder::setPosition(TimeUnit units, uint64_t position)
    {
        INIT_GUARD();

        const auto samples = std::round(
            convert(position, units, TimeUnit::PCM, m_spec) * 2.0);

        GME_ERR_CHECK(gme_seek_samples(m->emu, static_cast<int>(samples)));
        return true;
    }

    bool GmeDecoder::getPosition(TimeUnit units, double *outPosition) const
    {
        INIT_GUARD();

        const auto samples = gme_tell_samples(m->emu);

        if (outPosition)
            *outPosition = convert(samples / 2, TimeUnit::PCM, units, m_spec);
        return true;
    }

    void GmeDecoder::close() {
        if (m->emu)
        {
            gme_delete(m->emu);
            m->emu = nullptr;
        }
        m->stream.close();
    }

    bool GmeDecoder::open(const fs::path &path) {
        if (!m->stream.open(path))
        {
            return false;
        }

        // Find the GME file type
        auto pathStr = path.string();
        gme_type_t fileType = gme_identify_extension(pathStr.c_str());
        if (!fileType)
        {
            char header[4];
            if (m->stream.read(reinterpret_cast<uint8_t *>(header), sizeof(header)) < sizeof(header))
            {
                INSOUND_PUSH_ERROR(Result::UnexpectedData, "Failed to read file header, not a valid GME file");
                m->stream.close();
                return false;
            }
            if (!m->stream.seek(0))
            {
                INSOUND_PUSH_ERROR(Result::RuntimeErr, "Failed to seek back to stream start of GME file");
                m->stream.close();
                return false;
            }

            fileType = gme_identify_extension(gme_identify_header(header));
            if (!fileType)
            {
                INSOUND_PUSH_ERROR(Result::GmeErr, gme_wrong_file_type);
                return false;
            }
        }

        // Create the emulator
        const auto emu = gme_new_emu(fileType, m_spec.freq);
        if (!emu)
        {
            INSOUND_PUSH_ERROR(Result::GmeErr, "gme_new_emu_multi_channel failed");
            m->stream.close();
            return false;
        }

        // Load the stream
        if (const auto result = gme_load_custom(emu, &gme_read_rstream, m->stream.size(), m->stream.stream());
            result != nullptr)
        {
            INSOUND_PUSH_ERROR(Result::GmeErr, result);
            m->stream.close();
            gme_delete(emu);
            return false;
        }
        // Music_Emu *emu;
        // if (const auto result = gme_open_file(path.c_str(), &emu, m_spec.freq); result != nullptr)
        // {
        //     pushError(Result::GmeErr, result);
        //     return false;
        // }

        if (const auto result = gme_start_track(emu, 0); result != nullptr)
        {
            INSOUND_PUSH_ERROR(Result::GmeErr, result);
            gme_delete(emu);
            return false;
        }

        // Clean up any pre-existing emulator
        if (m->emu != nullptr)
        {
            gme_delete(m->emu);
        }

        m_spec.channels = 2;
        m_spec.format = SampleFormat(sizeof(short) * CHAR_BIT, false, false, true);
        m->emu = emu;
        m->trackNumber = 0;
        return true;
    }

}

