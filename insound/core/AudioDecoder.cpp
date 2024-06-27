#include "AudioDecoder.h"
#include "Error.h"

namespace insound {
#ifdef INSOUND_DEBUG
#define INIT_GUARD() do { if (!isOpen()) { \
        pushError(Result::DecoderNotInit, __FUNCTION__); \
        return false; \
    } } while(0)
#else
#define INIT_GUARD()
#endif

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
