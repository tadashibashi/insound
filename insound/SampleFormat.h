#pragma once
#include <cstdint>

namespace insound {
    /// Audio format, implementation of SDL audio format used as intermediary object for cross-compatibility
    class SampleFormat {
    public:
        /// Zero-ed null sample format object
        SampleFormat();
        SampleFormat(uint8_t bits, bool isFloat, bool isBigEndian, bool isSigned);

        /// @returns indicating format is floating point (on true), or integer data (on false)
        [[nodiscard]]
        bool isFloat() const;

        [[nodiscard]]
        bool isBigEndian() const;

        [[nodiscard]]
        bool isSigned() const;

        [[nodiscard]]
        uint8_t bits() const;

        [[nodiscard]]
        uint16_t flags() const { return m_flags; }
    private:
        uint16_t m_flags;
    };
}

