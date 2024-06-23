#pragma once
#include "Source.h"

#include <vector>

namespace insound {
    struct BusCommand;

    /// An audio bus that contains a list of audio sources and sums them into one output stream
    class Bus : public Source {
    public:
        explicit Bus();
        Bus(Bus &&other) noexcept;
        ~Bus() override = default;

        bool init(Engine *engine, const Handle<Bus> &parent, bool paused);

        /// Connect a Source to an output Bus.
        /// Caller must manage duplicates.
        /// TODO: this may be better left private, as it exposes danger of infinite bus recursion
        static bool connect(const Handle<Bus> &bus, const Handle<Source> &source);

        /// Remove source from this bus
        /// @note Source does not automatically call release when removed
        static bool disconnect(const Handle<Bus> &bus, const Handle<Source> &source);

        /// Get the parent output bus
        bool getOutputBus(Handle<Bus> *outBus);

    private: // Engine-accessible functions
        friend class Engine;

        // ----- Commands ----------------------------------------------
        bool updateParentClock(uint32_t parentClock) override;
        void processRemovals();
        void applyCommand(const BusCommand &command);

        /// Append a sound source to the bus.
        /// No checks for duplicates are made, caller should take care of this.
        /// Mix lock should be applied
        bool applyAppendSource(const Handle<Source> &handle);

        /// Explicitly remove a sound source from the bus.
        /// Mix lock should be applied.
        /// @returns whether source was removed - e.g. will return false if source does not belong to this bus.
        bool applyRemoveSource(const Handle<Source> &bus);

        int readImpl(uint8_t *output, int length) override;
        bool release(bool recursive);
        bool release() override;

    private: // Members
        std::vector<Handle<Source>> m_sources;
        AlignedVector<float, 16> m_buffer; ///< temp buffer to calculate mix
        Handle<Bus> m_parent;
        bool m_isMaster;
    };


}
