#include "SampleFormat.h"

namespace insound {
    SampleFormat::SampleFormat() : m_flags()
    {
    }

    SampleFormat::SampleFormat(const uint8_t bits, const bool isFloat, const bool isBigEndian, const bool isSigned) :
        m_flags()
    {
        m_flags += bits;
        m_flags |= (isFloat << 8) | (isBigEndian << 12) | (isSigned << 15);
    }

    bool SampleFormat::isFloat() const
    {
        return m_flags >> 8 & 1;
    }

    bool SampleFormat::isBigEndian() const
    {
        return m_flags >> 12 & 1;
    }

    bool SampleFormat::isSigned() const
    {
        return m_flags >> 15 & 1;
    }

    uint8_t SampleFormat::bits() const
    {
        return m_flags & 0xFFu;
    }
}


#include <insound/core/external/miniaudio.h>
uint32_t insound::toMaFormat(const insound::SampleFormat &spec)
{
    if (spec.isFloat())
    {
        return ma_format_f32;
    }

    switch(spec.bits())
    {
        case 8:
            return ma_format_u8;
        case 16:
            return ma_format_s16;
        case 24:
            return ma_format_s24;
        case 32:
            return ma_format_s32;
        default:
            return ma_format_unknown;
    }
}

