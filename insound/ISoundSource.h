#pragma once
#include <cstdint>
#include <vector>

namespace insound {
    struct Command;
    class Engine;
    class IEffect;

    class ISoundSource {
    public:
        explicit ISoundSource(Engine *engine, uint32_t parentClock, bool paused = false) :
            m_paused(paused), m_effects(), m_engine(engine), m_clock(), m_parentClock(parentClock)
        { }

        virtual ~ISoundSource();

        /// Get the current pointer position
        /// @param pcmPtr      pointer to receive pointer to data
        /// @param length      requested bytes
        /// @param parentClock caller's current clock time; the caller is usually the parent summing bus
        /// @returns the amount of bytes available or length arg, whichever is smaller
        int read(const uint8_t **pcmPtr, int length, uint32_t parentClock);

        [[nodiscard]]
        bool paused() const { return m_paused; }
        void paused(bool value);

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
    private:
        virtual int readImpl(uint8_t **pcmPtr, int length) = 0;
        bool m_paused;

        std::vector<IEffect * >m_effects;
        Engine *m_engine;
        uint32_t m_clock, m_parentClock;
    };
}
