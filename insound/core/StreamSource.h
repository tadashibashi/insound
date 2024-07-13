#pragma once
#include "Source.h"

#include "TimeUnit.h"

namespace insound {

    /// Streams various file types from disk
    class StreamSource : public Source {
    public:
        StreamSource();
        ~StreamSource() override;

        StreamSource(StreamSource &&other) noexcept;

        bool init(class Engine *engine, const std::string &filepath,
                  uint32_t parentClock, bool paused,
                  bool isLooping, bool isOneShot, bool inMemory = false);
        bool release() override;

        bool open(const std::string &filepath, bool inMemory = false);

        /// Open stram from const memory. Memory pointer must not be moved or invalidated for
        /// the duration that this source is used.
        bool openConstMem(const uint8_t *data, const size_t size);

        [[nodiscard]] bool isOpen() const;

        bool setLooping(bool looping);
        bool getLooping(bool *outLooping) const;

        bool getPosition(TimeUnit units, double *outPosition) const;
        bool setPosition(TimeUnit units, uint64_t position);
    private:
        int readImpl(uint8_t *output, int length) override;
        struct Impl;
        Impl *m;
    };

}

