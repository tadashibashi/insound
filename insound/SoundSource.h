#pragma once
#include <cstdint>
#include <vector>

namespace insound {
    class VolumeEffect;
    class PanEffect;
    struct Command;
    class Engine;
    class Effect;

    struct FadePoint {
        uint32_t clock;
        float value;
    };

    class SoundSource {
    public:
        explicit SoundSource(Engine *engine, uint32_t parentClock, bool paused = false);
        virtual ~SoundSource();

        /// Get the current pointer position
        /// @param pcmPtr      pointer to receive pointer to data
        /// @param length      requested bytes
        /// @returns the amount of bytes available or length arg, whichever is smaller
        int read(const uint8_t **pcmPtr, int length);

        [[nodiscard]]
        bool paused() const { return m_paused; }
        void paused(bool value, uint32_t clock = UINT32_MAX);

        /// Insert effect into effect chain, 0 is the first effect and effectCount - 1 is the last
        Effect *insertEffect(Effect *effect, int position);

        void removeEffect(Effect *effect);

        [[nodiscard]]
        auto effectCount() const { return m_effects.size(); }

        [[nodiscard]]
        Engine *engine() { return m_engine; }
        [[nodiscard]]
        const Engine *engine() const { return m_engine; }

        void applyCommand(const Command &command);

        [[nodiscard]]
        auto clock() const { return m_clock; }
        [[nodiscard]]
        auto parentClock() const { return m_parentClock; }

        virtual void updateParentClock(uint32_t parentClock) { m_parentClock = parentClock; }

        [[nodiscard]]
        PanEffect *panner() { return m_panner; }
        [[nodiscard]]
        const PanEffect *panner() const { return m_panner; }

        [[nodiscard]]
        float volume() const;
        void volume(float value);

        /// Add a linear fade point. Two must exist for it to be applied.
        /// @param clock parent clocks in samples
        /// @param value value to fade to
        void addFadePoint(uint32_t clock, float value);

        /// Remove fade points between start (inclusive) and end (exclusive), in parent clocks
        /// @param start starting point at which to remove fadepoints (inclusive)
        /// @param end   ending point at which to remove fadepoints (up until, but not including)
        void removeFadePoints(uint32_t start, uint32_t end = UINT32_MAX);

        bool getFadeValue(float *outValue) const;

        [[nodiscard]]
        bool shouldDiscard() const { return m_shouldDiscard; }

        virtual void release();
    private:
        virtual int readImpl(uint8_t *pcmPtr, int length) = 0;
        bool m_paused;

        std::vector<Effect * >m_effects;
        Engine *m_engine;
        uint32_t m_clock, m_parentClock;
        int m_pauseClock, m_unpauseClock;
        std::vector<uint8_t> m_outBuffer, m_inBuffer;

        std::vector<FadePoint> m_fadePoints;
        float m_fadeValue;

        // default effects included in each SoundSource
        PanEffect *m_panner;
        VolumeEffect *m_volume;
        bool m_shouldDiscard;
    };
}