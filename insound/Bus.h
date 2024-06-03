#pragma once
#include "SourceRefFwd.h"
#include "SoundSource.h"

#include <vector>

namespace insound {
    struct BusCommand;

    /// An audio bus that contains a list of audio sources and sums them into one output stream
    class Bus : public SoundSource {
    public:
        /// @note do not lock the mix thread before calling this, since this constructor defers a command which locks mix thread
        explicit Bus(Engine *engine, Bus *parent, bool paused);
        ~Bus() override = default;

        /// Release Bus from audio engine graph; invalidates it from further use
        /// (results in Result::InvalidHandle error)
        /// @param recursive
        bool release(bool recursive);

        /// Release only the bus, not its sources (sources get attached to the master bus instead)
        bool release() override; // overriden to reattach sources to master bus

        /// Set this bus's parent output bus
        bool setOutput(BusRef output);

        /// Caller must manage duplicates
        bool appendSource(SoundSourceRef &source);

        /// Remove source from this bus
        /// @note Source does not automatically call release when removed
        bool removeSource(SoundSourceRef &source);

    private: // Engine-accessible functions
        friend class Engine;

        // ----- Master bus lifetime -----------------------------------
        static std::shared_ptr<Bus> createMasterBus(Engine *engine);
        static bool releaseMasterBus(Bus *masterBus);

        // ----- Commands ----------------------------------------------
        bool updateParentClock(uint32_t parentClock) override;
        void processRemovals();
        void applyCommand(const BusCommand &command);
        bool setOutputImpl(Bus *output);
        bool appendSourceImpl(SoundSource *source);

        /// Append a sound source to the bus.
        /// No checks for duplicates are made, caller should take care of this.
        /// Mix lock should be applied
        bool applyAppendSource(SoundSource *source);

        /// Explicitly remove a sound source from the bus.
        /// Mix lock should be applied.
        /// @returns whether source was removed - e.g. will return false if source does not belong to this bus.
        bool applyRemoveSource(const SoundSource *source);

        int readImpl(uint8_t *output, int length) override;

    private: // Members
        std::vector<SoundSource *> m_sources;
        std::vector<float> m_buffer; ///< temp buffer to calculate mix
        Bus *m_parent;
        bool m_isMaster;
    };


}
