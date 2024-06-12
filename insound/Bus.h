#pragma once
#include "SourceRefFwd.h"
#include "Source.h"

#include <vector>

#include "Pool.h"
#include "SourcePool.h"

namespace insound {
    struct BusCommand;

    /// An audio bus that contains a list of audio sources and sums them into one output stream
    class Bus : public Source {
    public:
        /// @note do not lock the mix thread before calling this, since this constructor defers a command which locks mix thread
        explicit Bus(Engine *engine, Handle<Bus> parent, bool paused);
        ~Bus() override = default;

        /// Set this bus's parent output bus
        static bool setOutput(Handle<Bus> bus, Handle<Bus> output);

        /// Add source to bus.
        /// Caller must manage duplicates.
        /// TODO: this may be better left private, as it exposes danger of infinite bus recursion
        static bool appendSource(Handle<Bus> bus, Handle<Source> source);

        /// Remove source from this bus
        /// @note Source does not automatically call release when removed
        static bool removeSource(Handle<Bus> bus, Handle<Source> source);



    private: // Engine-accessible functions
        friend class Engine;

        // ----- Commands ----------------------------------------------
        bool updateParentClock(uint32_t parentClock) override;
        void processRemovals();
        void applyCommand(BusCommand &command);

        /// Append a sound source to the bus.
        /// No checks for duplicates are made, caller should take care of this.
        /// Mix lock should be applied
        bool applyAppendSource(Handle<Source> handle);

        /// Explicitly remove a sound source from the bus.
        /// Mix lock should be applied.
        /// @returns whether source was removed - e.g. will return false if source does not belong to this bus.
        bool applyRemoveSource(Handle<Source> bus);

        int readImpl(uint8_t *output, int length) override;
        bool release(bool recursive);
        bool release() override;
        
    private: // Members


        std::vector<Handle<Source>> m_sources;
        std::vector<float> m_buffer; ///< temp buffer to calculate mix
        Handle<Bus> m_parent;
        bool m_isMaster;
    };


}
