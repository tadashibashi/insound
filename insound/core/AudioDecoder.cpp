#include "AudioDecoder.h"
#include "Error.h"

#include "io/decodeGME.h"
#include "io/decodeMp3.h"
#include "io/decodeWAV.h"

namespace insound {
#ifdef INSOUND_DEBUG
#define INIT_GUARD() do { if (!isOpen()) { \
        INSOUND_PUSH_ERROR(Result::DecoderNotInit, __FUNCTION__); \
        return false; \
    } } while(0)
#else
#define INIT_GUARD()
#endif

    AudioDecoder *AudioDecoder::create(const fs::path &filepath, const AudioSpec &targetSpec)
    {
        AudioDecoder *decoder = nullptr;

        // Open decoder by file extension
        auto ext = filepath.extension().string();
        for (auto &c : ext) // capitalize for a uniform check
            c = static_cast<char>(std::toupper(c));

        if (ext == ".WAV" || ext == ".WAVE" || ext == ".AIFF" || ext == ".W64")
        {
            decoder = new WavDecoder();
        }
        else if (ext == ".MP3")
        {
#if INSOUND_DECODE_MP3
            decoder = new Mp3Decoder();
#else
            INSOUND_PUSH_ERROR(Result::NotSupported, "Mp3 decoding is not supported, "
                      "make sure to compile with INSOUND_DECODE_MP3 defined");
            return false;
#endif      // TODO: add the other decoder types here
        }
#if INSOUND_DECODE_GME
        else
        {
            decoder = new GmeDecoder(targetSpec.freq);
        }
#endif

        if (decoder == nullptr)
        {
            INSOUND_PUSH_ERROR(Result::NotSupported, "File extension is not supported");
        }

        return decoder;
    }

    int AudioDecoder::readBytes(const int bytesToRead, uint8_t *buffer)
    {
        const auto k = (m_spec.format.bits() / CHAR_BIT) * m_spec.channels;
        const auto sampleFrames = bytesToRead / k;
        return read(sampleFrames, buffer) * k;
    }

    bool AudioDecoder::getLooping(bool *outLooping) const
    {
        INIT_GUARD();
        if (outLooping)
            *outLooping = m_looping;
        return true;
    }

    bool AudioDecoder::setLooping(const bool value)
    {
        INIT_GUARD();
        m_looping = value;
        return true;
    }

    bool AudioDecoder::getSpec(AudioSpec *outSpec) const
    {
        INIT_GUARD();
        if (outSpec)
            *outSpec = m_spec;
        return true;
    }
}
