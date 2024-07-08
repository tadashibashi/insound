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
        [[nodiscard]] bool isOpen() const;

        void queueNextBuffer();

        bool setLooping(bool looping);
        bool getLooping(bool *outLooping) const;

        bool getPosition(TimeUnit units, double *outPosition) const;
        bool setPosition(TimeUnit units, uint64_t position);

        /// Whether enough data has buffered to begin playing.
        [[nodiscard]]
        bool isReady(bool *outReady) const;

    private:
        /// Bytes available to consume in the stream
        [[nodiscard]]
        int bytesAvailable() const;

        int readImpl(uint8_t *output, int length) override;
        struct Impl;
        Impl *m;
    };

}

