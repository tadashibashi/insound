#include "Engine.h"

#include "AlignedVector.h"
#include "AudioDevice.h"
#include "AudioSpec.h"
#include "Bus.h"
#include "Command.h"
#include "Effect.h"
#include "Error.h"
#include "lib.h"
#include "PCMSource.h"
#include "SoundBuffer.h"
#include "StreamSource.h"
#include "Source.h"

#include <mutex>
#include <vector>

namespace insound {
#ifdef INSOUND_DEBUG
/// Checks that engine is open before performing a function.
/// Return type of the function called in most be `bool`
#define ENGINE_INIT_GUARD() do { if (!isOpen()) { \
        INSOUND_PUSH_ERROR(Result::EngineNotInit, __FUNCTION__); \
        return false; \
    } } while(0)
#else
#define ENGINE_INIT_GUARD()
#endif

    struct Engine::Impl {
    public:
        explicit Impl(Engine *engine) : m_engine(engine), m_clock(), m_masterBus(),
                                        m_device(), m_deferredCommands(),
                                        m_immediateCommands(),
                                        m_discardFlag(false), m_immediateCommandMutex(), m_deferredCommandMutex(), m_mixMutex()
        {
            m_device = AudioDevice::create();
        }

        ~Impl()
        {
            close(); // close the device and clean up mix graph

            AudioDevice::destroy(m_device); // delete the device
        }

        bool open(const int frequency = 0, const int samples = 1024)
        {
            if (!m_device->open(frequency, samples, &Impl::audioCallback, this))
            {
                return false;
            }

            Handle<Bus> busHandle;
            if (!createBus(false, {}, &busHandle, true))
            {
                m_device->close();
                return false;
            }

            m_masterBus = busHandle;
            m_device->resume();
            return true;
        }

        void close()
        {
            if (isOpen())
            {
                if (m_masterBus.isValid())
                {
                    auto lockGuard = std::lock_guard(m_mixMutex);
                    m_masterBus->m_isMaster = false; // enable bus deletion
                    release(
                        static_cast<Handle<Source>>(m_masterBus), true);

                    processCommands(this, m_immediateCommands); // flush command buffers
                    processCommands(this, m_deferredCommands);

                    m_masterBus->processRemovals();
                    m_engine->destroySource(
                        static_cast<Handle<Source>>(m_masterBus));
                    m_masterBus = {};
                }

                m_clock = 0;
                m_device->close();
            }
        }

        [[nodiscard]]
        bool getBufferSize(uint32_t *outSize) const
        {
            ENGINE_INIT_GUARD();

            if (outSize)
                *outSize = m_device->bufferSize();

            return true;
        }

        [[nodiscard]]
        bool isOpen() const
        {
            return m_device->isOpen();
        }

        /// Pass null bus with default constructor `{}`
        bool playSound(const SoundBuffer *buffer, bool paused, bool looping,
                       bool oneshot, const Handle<Bus> &bus,
                       Handle<PCMSource> *outPcmSource)
        {
            ENGINE_INIT_GUARD();
            std::lock_guard lockGuard(m_mixMutex);
            if (!buffer || !buffer->isLoaded())
            {
                INSOUND_PUSH_ERROR(Result::InvalidSoundBuffer, "Failed to play sound");
                return false;
            }

            if (bus && !bus.isValid()) // if output bus was passed, and it's invalid => error
            {
                INSOUND_PUSH_ERROR(Result::InvalidHandle,
                          "Engine::Impl::createBus failed because output Bus was invalid");
                return false;
            }

            uint32_t clock;
            bool result;
            if (bus)
                result = bus->getClock(&clock);
            else
                result = m_masterBus->getClock(&clock);

            if (!result)
            {
                return false;
            }

            const auto newSource = m_objectPool.allocate<PCMSource>(
                m_engine, buffer, clock, paused, looping, oneshot);

            pushImmediateCommand(
                Command::makeBusAppendSource(bus ? bus : m_masterBus, newSource.cast<Source>()));

            if (outPcmSource)
                *outPcmSource = newSource;

            return true;
        }

        bool playStream(const std::string &filepath, bool paused, bool looping,
                        bool oneshot, bool inMemory, const Handle<Bus> &bus,
                        Handle<StreamSource> *outSource)
        {
            ENGINE_INIT_GUARD();

#if INSOUND_TARGET_EMSCRIPTEN // emscripten can't stream from file
            inMemory = true;
#endif

            std::lock_guard lockGuard(m_mixMutex);
            uint32_t clock;
            bool result = bus ?
                bus->getClock(&clock) : m_masterBus->getClock(&clock);

            if (!result)
                return false;

            const auto newSource = m_objectPool.allocate<StreamSource>(
                m_engine, filepath, clock, paused, looping, oneshot, inMemory);

            pushImmediateCommand(
                Command::makeBusAppendSource(bus ? bus : m_masterBus, newSource.cast<Source>()));

            if (outSource)
                *outSource = newSource;

            return true;
        }

        bool createBus(bool paused, const Handle<Bus> &output, Handle<Bus> *outBus, const bool isMaster)
        {
            ENGINE_INIT_GUARD();
            std::lock_guard lockGuard(m_mixMutex);
            if (output && !output.isValid()) // if output was passed, and it's invalid => error
            {
                INSOUND_PUSH_ERROR(Result::InvalidHandle, "Engine::Impl::createBus failed because output Bus was invalid");
                return false;
            }

            // Determine the output bus
            auto outputBus =  isMaster ? Handle<Bus>{} :  // if creating the master bus => no output
                output.isValid() ? output : m_masterBus;  // otherwise use provided output or master if null

            const auto newBusHandle = m_objectPool.allocate<Bus>(
                m_engine,
                outputBus,
                paused);

            // Connect bus to output
            if (outputBus)
                pushImmediateCommand(
                    Command::makeBusAppendSource(outputBus, newBusHandle.cast<Source>()));

            if (isMaster) // flag master
                newBusHandle->m_isMaster = true;

            if (outBus)
            {
                *outBus = newBusHandle;
            }

            return true;
        }

        bool release(const Handle<Source> &source, const bool recursive)
        {
            ENGINE_INIT_GUARD();

            if (!source.isValid())
            {
                INSOUND_PUSH_ERROR(Result::InvalidHandle, "Engine::releaseBus: bus was invalid");
                return false;
            }

            m_discardFlag = true;
            return pushCommand(
                Command::makeEngineDeallocateSource(m_engine, source, recursive));
        }

        bool releaseRaw(Source *source, const bool recursive)
        {
            ENGINE_INIT_GUARD();

            m_discardFlag = true;
            return pushCommand(
                Command::makeEngineDeallocateSourceRaw(m_engine, source, recursive));
        }

        /// Non-zero positive number if open, zero if not
        [[nodiscard]]
        bool deviceID(uint32_t *outDeviceID) const
        {
            ENGINE_INIT_GUARD();

            if (outDeviceID)
                *outDeviceID = m_device->id();
            return true;
        }

        /// Get audio specification data
        /// @param outSpec spec to receive
        /// @returns true on success, false on error
        bool getSpec(AudioSpec *outSpec) const
        {
            ENGINE_INIT_GUARD();

            if (outSpec)
            {
                *outSpec = m_device->spec();
            }

            return true;
        }

        bool getMasterBus(Handle<Bus> *outBus) const
        {
            ENGINE_INIT_GUARD();

            if (outBus)
            {
                *outBus = m_masterBus;
            }

            return true;
        }

        bool getPaused(bool *outValue) const
        {
            ENGINE_INIT_GUARD();

            if (outValue)
            {
                *outValue = !m_device->isRunning();
            }

            return true;
        }

        bool setPaused(const bool value)
        {
            ENGINE_INIT_GUARD();

            if (value)
                m_device->suspend();
            else
                m_device->resume();
            return true;
        }

        bool update()
        {
            ENGINE_INIT_GUARD();
            m_device->update();

            {
                auto deferredCommandGuard = std::lock_guard(m_deferredCommandMutex);
                if (!m_deferredCommands.empty())
                {
                    if (m_mixMutex.try_lock())
                    {
                        try
                        {
                            processCommands(this, m_deferredCommands);
                        }
                        catch(...)
                        {
                            m_mixMutex.unlock();
                            throw;
                        }
                        m_mixMutex.unlock();
                    }

                }
            }

            if (m_discardFlag)
            {
                if (m_masterBus.isValid()) // is this necessary?
                {
                    auto lockGuard = std::lock_guard(m_mixMutex);
                    if (m_mixMutex.try_lock())
                    {
                        try
                        {
                            m_masterBus->processRemovals();
                        }
                        catch(...)
                        {
                            m_mixMutex.unlock();
                            throw;
                        }

                        m_discardFlag = false;
                        m_mixMutex.unlock();
                    }

                }
                else
                {
                    INSOUND_PUSH_ERROR(Result::InvalidHandle, "Internal error: master bus is invalidated");
                    return false;
                }

            }

            return true;
        }

        bool pushCommand(const Command &command)
        {
            ENGINE_INIT_GUARD();

            std::lock_guard lockGuard(m_deferredCommandMutex);
            m_deferredCommands.emplace_back(command);
            return true;
        }

        bool pushImmediateCommand(const Command &command)
        {
            ENGINE_INIT_GUARD();

            std::lock_guard lockGuard(m_immediateCommandMutex);
            m_immediateCommands.emplace_back(command);
            return true;
        }

        void processCommand(const EngineCommand &command)
        {
            // This is called from the mix thread, so we shouldn't lock

            switch(command.type)
            {
                case EngineCommand::ReleaseSource:
                {
                    auto &source = command.deallocsource.source;
                    if (!source)
                        break;

                    if (const auto bus = source.getAs<Bus>())
                    {
                        bus->release(command.deallocsource.recursive);
                    }
                    else
                    {
                        source->release();
                    }

                    m_discardFlag = true;
                } break;

                case EngineCommand::ReleaseSourceRaw:
                {
                    const auto source = command.deallocsourceraw.source;
                    if (!source)
                        break;

                    if (const auto bus = dynamic_cast<Bus *>(source))
                    {
                        bus->release(command.deallocsourceraw.recursive);
                    }
                    else
                    {
                        source->release();
                    }

                    m_discardFlag = true;
                } break;

                default:
                {
                    // unknown engine command
                } break;
            }
        }

        [[nodiscard]]
        const MultiPool &getObjectPool() const
        {
            return m_objectPool;
        }

        [[nodiscard]]
        MultiPool &getObjectPool()
        {
            return m_objectPool;
        }

        AudioDevice &getAudioDevice()
        {
            return *m_device;
        }

        std::lock_guard<std::mutex> mixLockGuard() { return std::lock_guard(m_mixMutex); }
    private:

        /// Process a vector of commands
        /// @param engine   context object, we may not need it
        /// @param commands commands to apply
        static void processCommands([[maybe_unused]] const Impl *engine,
            std::vector<Command> &commands)
        {
            if (commands.empty())
                return;

            // Call `applyCommand` on the target object by type
            for (auto &command : commands)
            {
                switch(command.type)
                {
                    case Command::Engine:
                    {
                        command.engine.engine->applyCommand(command.engine);
                    } break;

                    case Command::Effect:
                    {
                        command.effect.effect->applyCommand(command.effect);
                    } break;

                    case Command::Source:
                    {
                        command.source.source->applyCommand(command.source);
                    } break;

                    case Command::PCMSource:
                    {
                        command.pcmsource.source->applyCommand(command.pcmsource);
                    } break;

                    case Command::Bus:
                    {
                        command.bus.bus->applyCommand(command.bus);
                    } break;

                    default:
                    {

                    } break;
                }
            }

            commands.clear();
        }

        /// Audio callback to pass to the device
        /// @param userptr context object
        /// @param outBuffer buffer to fill or swap, as long as the lengths are equal
        static void audioCallback(void *userptr, AlignedVector<uint8_t, 16> *outBuffer)
        {
            const auto engine = static_cast<Impl *>(userptr);
            if (!engine->isOpen() || !engine->m_masterBus)
                return;

            auto guard = std::lock_guard(engine->m_mixMutex);
            // Process commands that require sample-accurate immediacy
            {
                auto deferredCommandGuard = std::lock_guard(engine->m_deferredCommandMutex);
                if (!engine->m_immediateCommands.empty())
                    Engine::Impl::processCommands(engine, engine->m_immediateCommands);
            }
            const auto size = outBuffer->size();
            engine->m_masterBus->read(nullptr, static_cast<int>(size));
            engine->m_clock += size / (2 * sizeof(float));
            engine->m_masterBus->updateParentClock(engine->m_clock);

            engine->m_masterBus->swapBuffers(outBuffer);

            // if (engine->m_mixMutex.try_lock())
            // {
            //     // Process commands that require sample-accurate immediacy
            //     {
            //         auto deferredCommandGuard = std::lock_guard(engine->m_deferredCommandMutex);
            //         if (!engine->m_immediateCommands.empty())
            //             Engine::Impl::processCommands(engine, engine->m_immediateCommands);
            //     }
            //
            //     try
            //     {
            //         const auto size = outBuffer->size();
            //
            //         engine->m_masterBus->read(nullptr, static_cast<int>(size));
            //         engine->m_clock += size / (2 * sizeof(float));
            //         engine->m_masterBus->updateParentClock(engine->m_clock);
            //
            //         engine->m_masterBus->swapBuffers(outBuffer);
            //     }
            //     catch(const std::exception &e)
            //     {
            //         INSOUND_PUSH_ERROR(Result::StdExcept, e.what());
            //         engine->m_mixMutex.unlock();
            //         throw;
            //     }
            //     catch(...)
            //     {
            //         INSOUND_PUSH_ERROR(Result::StdExcept, "unknown exception thrown during engine mixing");
            //         engine->m_mixMutex.unlock();
            //         throw;
            //     }
            //
            //     engine->m_mixMutex.unlock();
            // }
            // else
            // {
            //     INSOUND_LOG("Engine mix callback is busy\n");
            // }

        }

        Engine *m_engine;
        uint32_t m_clock;

        Handle<Bus> m_masterBus;
        AudioDevice *m_device;

        std::vector<Command> m_deferredCommands;
        std::vector<Command> m_immediateCommands;

        bool m_discardFlag; ///< set to true when sound source discard should be made
        MultiPool m_objectPool; ///< manages multiple pools of different source types (also effects)

        std::mutex m_immediateCommandMutex;
        std::mutex m_deferredCommandMutex;
        mutable std::mutex m_mixMutex;
    };

    Engine::Engine() : m(new Impl(this))
    { }

    Engine::~Engine()
    {
        delete m;
    }

    bool Engine::open(const int samplerate, const int bufferFrameSize)
    {
        return m->open(samplerate, bufferFrameSize);
    }

    void Engine::close()
    {
        m->close();
    }

    bool Engine::isOpen() const
    {
        return m->isOpen();
    }

    bool Engine::playSound(const SoundBuffer *buffer, const bool paused, const bool looping,
                           const bool oneshot, const Handle<Bus> &bus, Handle<PCMSource> *outPcmSource)
    {
        return m->playSound(buffer, paused, looping, oneshot, bus, outPcmSource);
    }

    bool Engine::playSound(const SoundBuffer *buffer, const bool paused, const bool looping,
        const bool oneshot, Handle<PCMSource> *outPcmSource)
    {
        return m->playSound(buffer, paused, looping, oneshot, {}, outPcmSource);
    }

    bool Engine::createBus(const bool paused, const Handle<Bus> &output, Handle<Bus> *outBus)
    {
        return m->createBus(paused, output, outBus, false);
    }

    bool Engine::createBus(const bool paused, Handle<Bus> *outBus)
    {
        return m->createBus(paused, {}, outBus, false);
    }

    std::lock_guard<std::mutex> Engine::mixLockGuard()
    {
        return m->mixLockGuard();
    }

    bool Engine::releaseSoundImpl(const Handle<Source> &source)
    {
        return m->release(source, false);
    }

    bool Engine::releaseSoundRaw(Source *source, const bool recursive)
    {
        return m->releaseRaw(source, recursive);
    }

    bool Engine::getDeviceID(uint32_t *outDeviceID) const
    {
        return m->deviceID(outDeviceID);
    }

    bool Engine::getSpec(AudioSpec *outSpec) const
    {
        return m->getSpec(outSpec);
    }

    bool Engine::getBufferSize(uint32_t *outSize) const
    {
        return m->getBufferSize(outSize);
    }

    bool Engine::getMasterBus(Handle<Bus> *outBus) const
    {
        return m->getMasterBus(outBus);
    }

    bool Engine::pushCommand(const Command &command)
    {
        return m->pushCommand(command);
    }

    bool Engine::pushImmediateCommand(const Command &command)
    {
        return m->pushImmediateCommand(command);
    }

    bool Engine::setPaused(const bool value)
    {
        return m->setPaused(value);
    }

    bool Engine::getPaused(bool *outValue) const
    {
        return m->getPaused(outValue);
    }

    bool Engine::update()
    {
        return m->update();
    }

    void Engine::destroySource(const Handle<Source> &source)
    {
        m->getObjectPool().deallocate(source);
    }

    const MultiPool &Engine::getObjectPool() const
    {
        return m->getObjectPool();
    }

    MultiPool &Engine::getObjectPool()
    {
        return m->getObjectPool();
    }

    AudioDevice &Engine::device()
    {
        return m->getAudioDevice();
    }

    void Engine::applyCommand(const EngineCommand &command)
    {
        m->processCommand(command);
    }

    bool Engine::playStream(const std::string &filepath, bool paused, bool looping,
        bool oneshot, bool inMemory, const Handle<Bus> &bus, Handle<StreamSource> *outSource)
    {
        return m->playStream(filepath, paused, looping, oneshot, inMemory, bus, outSource);
    }

}
