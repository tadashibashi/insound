#pragma once

#include "ISoundSource.h"
#include "effects/PanEffect.h"

#include <vector>

namespace insound {
    class Bus : public ISoundSource {
    public:
        explicit Bus(Engine *engine, Bus *parent);
        ~Bus() override = default;

        /// Append a sound source to the bus.
        /// No checks for duplicates are made, caller should take care of this.
        /// Mix lock should be applied
        void appendSource(ISoundSource *source);

        /// Explicitly remove a sound source from the bus.
        /// Mix lock should be applied.
        /// @returns whether source was removed - e.g. will return false if source does not belong to this bus.
        bool removeSource(const ISoundSource *source);

        /// Process removals.
        /// Mix lock should be appied.
        void update();

        void updateParentClock(uint32_t parentClock) override;
    private:

        int readImpl(uint8_t *pcmPtr, int length) override;
        std::vector<ISoundSource *> m_sources;
        std::vector<float> m_buffer; ///< temp buffer to calculate mix
        Bus *m_parent;
        PanEffect *m_panner;
    };
}
