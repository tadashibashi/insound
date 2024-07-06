#include "SampleFormat.h"

#include <climits>

#include "util.h"

namespace insound {
    SampleFormat::SampleFormat() : m_flags()
    {
    }

    SampleFormat::SampleFormat(const uint32_t bits, const bool isFloat, const bool isBigEndian, const bool isSigned) :
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

    uint32_t SampleFormat::bits() const
    {
        return m_flags & 0xFFu;
    }

    uint32_t SampleFormat::bytes() const
    {
        return bits() / CHAR_BIT;
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

static int maFormatToBytes(const int format)
{
    switch(format)
    {
        case ma_format_f32: case ma_format_s32:
            return 4;
        case ma_format_s24:
            return 3;
        case ma_format_s16:
            return 2;
        case ma_format_u8:
            return 1;
        default:
            return 0;
    }
}

insound::SampleFormat insound::toFormat(int maFormat)
{
    return {
        static_cast<uint32_t>(maFormatToBytes(maFormat) * CHAR_BIT),
        maFormat == ma_format_f32,
        endian::native == endian::big,
        maFormat != ma_format_u8
    };
}
