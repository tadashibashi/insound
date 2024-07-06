#pragma once
#include <cstdint>

namespace insound {
    /// Description of sample data type (e.g. int16, float32, etc.). It is an implementation of SDL audio format and used as intermediary object for cross-compatibility
    class SampleFormat {
    public:
        /// Zero-ed null sample format object
        SampleFormat();
        SampleFormat(uint32_t bits, bool isFloat, bool isBigEndian, bool isSigned);

        /// @returns whether format is floating point (on true), or integer data (on false)
        [[nodiscard]]
        bool isFloat() const;

        /// @returns whether format is big endian (on true), or little endian (on false)
        [[nodiscard]]
        bool isBigEndian() const;

        /// @returns whether format is signed (on true), or unsigned (on false)
        [[nodiscard]]
        bool isSigned() const;

        /// @returns how many bits per sample - divide by CHAR_BIT to get bytes
        [[nodiscard]]
        uint32_t bits() const;

        [[nodiscard]]
        uint32_t bytes() const;

        /// @returns the raw flags
        [[nodiscard]]
        uint16_t flags() const { return m_flags; }
    private:
        uint16_t m_flags;
    };

    [[nodiscard]]
    uint32_t toMaFormat(const SampleFormat &spec);
    [[nodiscard]]
    /// ma format to format converter
    SampleFormat toFormat(int maFormat);
}
