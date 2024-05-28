#pragma once
#include <cstdint>
#include <vector>

namespace insound {
    class VolumeEffect;
    class PanEffect;
    struct Command;
    class Engine;
    class IEffect;

    class ISoundSource {
    public:
        explicit ISoundSource(Engine *engine, uint32_t parentClock, bool paused = false);
        virtual ~ISoundSource();

        /// Get the current pointer position
        /// @param pcmPtr      pointer to receive pointer to data
        /// @param length      requested bytes
        /// @returns the amount of bytes available or length arg, whichever is smaller
        int read(const uint8_t **pcmPtr, int length);

        [[nodiscard]]
        bool paused() const { return m_paused; }
        void paused(bool value, uint32_t clock = UINT32_MAX);

        /// Insert effect into effect chain, 0 is the first effect and effectCount - 1 is the last
        IEffect *insertEffect(IEffect *effect, int position);

        void removeEffect(IEffect *effect);

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
    private:
        virtual int readImpl(uint8_t *pcmPtr, int length) = 0;
        bool m_paused;

        std::vector<IEffect * >m_effects;
        Engine *m_engine;
        uint32_t m_clock, m_parentClock;
        int m_pauseClock, m_unpauseClock;
        std::vector<uint8_t> m_outBuffer, m_inBuffer;

        PanEffect *m_panner;
        VolumeEffect *m_volume;
    };
}
