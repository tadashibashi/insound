#pragma once

#include "SoundSource.h"
#include "effects/PanEffect.h"

#include <vector>

namespace insound {
    class Bus : public SoundSource {
    public:
        explicit Bus(Engine *engine, Bus *parent);
        ~Bus() override = default;

        void release(bool recursive);

        // void setOutput(Bus *output);

    private: // Engine-accessible functions
        friend class Engine;
        void updateParentClock(uint32_t parentClock) override;
        void processRemovals();
        /// Append a sound source to the bus.
        /// No checks for duplicates are made, caller should take care of this.
        /// Mix lock should be applied
        void appendSource(SoundSource *source);

        /// Explicitly remove a sound source from the bus.
        /// Mix lock should be applied.
        /// @returns whether source was removed - e.g. will return false if source does not belong to this bus.
        bool removeSource(const SoundSource *source);

        int readImpl(uint8_t *pcmPtr, int length) override;

    private: // Members
        std::vector<SoundSource *> m_sources;
        std::vector<float> m_buffer; ///< temp buffer to calculate mix
        Bus *m_parent;
        PanEffect *m_panner;
    };
}
