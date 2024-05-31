#pragma once

#include "SoundSource.h"
#include "effects/PanEffect.h"

#include <vector>

#include "SourceHandle.h"

namespace insound {
    struct Command;

    class Bus : public SoundSource {
    public:
        explicit Bus(Engine *engine, Bus *parent, bool paused);
        ~Bus() override = default;

        void release(bool recursive);

        void setOutput(SourceHandle<Bus> output);

    private: // Engine-accessible functions
        friend class Engine;
        void updateParentClock(uint32_t parentClock) override;
        void processRemovals();
        void applyBusCommand(const Command &command);
        void setOutput(Bus *output);

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
    };
}
