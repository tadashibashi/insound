#pragma once
#include "SourceRefFwd.h"

#include "MultiPool.h"

#include <cstdint>

#include "AudioDevice.h"

namespace insound {
    class BufferPool;
    struct AudioSpec;
    class Bus;
    struct Command;
    struct EngineCommand;
    class SoundBuffer;
    class Source;
    class PCMSource;

    class Engine {
    public:
        Engine();
        ~Engine();

        bool open(int samplerate, int bufferFrameSize);
        void close();

        [[nodiscard]]
        bool isOpen() const;

        /// Play a sound
        /// @param buffer  buffer to play
        /// @param paused  whether to start sound paused; useful if you wish to modify the source before it plays
        /// @param looping whether to loop sound
        /// @param oneshot whether to release sound resources at end, (will not auto-release while looping is true)
        /// @param bus     bus this source should output to; left null the master bus will be used
        /// @param outPcmSource pointer to receive pcm source, nullable if you don't need it e.g. a oneshot sound
        bool playSound(const SoundBuffer *buffer, bool paused, bool looping, bool oneshot, Handle<Bus> bus, Handle<PCMSource> *outPcmSource);
        bool playSound(const SoundBuffer *buffer, bool paused, bool looping, bool oneshot, Handle<PCMSource> *outPcmSource);

        /// Create a new bus to use in the mixing graph
        /// @param paused whether bus should start off paused on initialization
        /// @param output output bus to feed this bus to, if nullptr, the master Bus will be used
        /// @param outBus [out] pointer to retrieve created bus reference
        bool createBus(bool paused, const Handle<Bus> &output, Handle<Bus> *outBus);
        bool createBus(bool paused, Handle<Bus> *outBus);

        template <typename T>
        bool releaseSound(Handle<T> source)
        {
            return releaseSoundImpl((Handle<Source>)source);
        }

        /// Release a bus
        /// @param bus bus handle to release
        /// @param recursive whether to recursively release all bus's children, if `false` all children will be
        ///                  reattached to the master bus instead. If `true` all children will be released / deleted.
        bool releaseBus(const Handle<Bus> &bus, bool recursive);

        /// Retrieve the engine's device ID. If zero, the audio device is uninitialized.
        [[nodiscard]]
        bool getDeviceID(uint32_t *outDeviceID) const;

        /// Get audio spec
        bool getSpec(AudioSpec *outSpec) const;

        /// Get the size of audio buffer in bytes
        bool getBufferSize(uint32_t *outSize) const;

        /// Get the master bus
        bool getMasterBus(Handle<Bus> *outBus) const;

        /// Push a command to be deferred until `update` is called
        bool pushCommand(const Command &command);

        /// Push a command that will immediately be processed the next audio buffer.
        /// This is for commands that are sample clock-sensitive.
        bool pushImmediateCommand(const Command &command);

        /// Pause the audio device
        /// @returns whether function succeeded, check `popError()` for details
        bool setPaused(bool value);

        /// Get if the device is paused
        bool getPaused(bool *outValue) const;

        bool update();

        template <typename T>
        bool tryFindHandle(T *ptr, Handle<T> *outHandle)
        {
            return getObjectPool().tryFind(ptr, outHandle);
        }

        struct Impl;
    private:
        friend class Bus;
        friend class Source;

        template <typename T, typename...TArgs>
        Handle<T> createObject(TArgs &&...args)
        {
            return getObjectPool().allocate<T>(std::forward<TArgs>(args)...);
        }

        bool releaseSoundImpl(Handle<Source> source);
        void destroySource(const Handle<Source> &source);
        [[nodiscard]]
        const MultiPool &getObjectPool() const;
        [[nodiscard]]
        MultiPool &getObjectPool();

        AudioDevice &device();

        void applyCommand(EngineCommand &command);
        Impl *m;
    };
}
