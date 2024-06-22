#include "Engine.h"

#include <iostream>

#include "AlignedVector.h"
#include "AudioDevice.h"
#include "AudioSpec.h"
#include "Bus.h"
#include "Command.h"
#include "Effect.h"
#include "Error.h"
#include "PCMSource.h"
#include "SoundBuffer.h"
#include "Source.h"

#include <mutex>
#include <vector>

namespace insound {
#ifdef INSOUND_DEBUG
/// Checks that engine is open before performing a function.
/// Return type of the function called in most be `bool`
#define ENGINE_INIT_GUARD() do { if (!isOpen()) { \
        pushError(Result::EngineNotInit, __FUNCTION__); \
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
                                        m_discardFlag(false), m_immediateCommandMutex(), m_deferredCommandMutex()
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
            if (!m_device->open(frequency, samples, Impl::audioCallback, this))
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
                    auto lockGuard = m_device->mixLockGuard();
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
        bool playSound(const SoundBuffer *buffer, bool paused, bool looping, bool oneshot, const Handle<Bus> &bus, Handle<PCMSource> *outPcmSource)
        {
            ENGINE_INIT_GUARD();

            if (!buffer || !buffer->isLoaded())
            {
                pushError(Result::InvalidSoundBuffer, "Failed to play sound");
                return false;
            }

            auto lockGuard = m_device->mixLockGuard();
            if (bus && !bus.isValid()) // if output bus was passed, and it's invalid => error
            {
                pushError(Result::InvalidHandle, "Engine::Impl::createBus failed because output Bus was invalid");
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
            if (bus)
            {
                bus->applyAppendSource(
                    static_cast<Handle<Source>>(newSource));
            }
            else
            {
                m_masterBus->applyAppendSource(
                    static_cast<Handle<Source>>(newSource));
            }

            if (outPcmSource)
                *outPcmSource = newSource;

            return true;
        }

        bool createBus(bool paused, const Handle<Bus> &output, Handle<Bus> *outBus, const bool isMaster)
        {
            ENGINE_INIT_GUARD();
            auto lockGuard = m_device->mixLockGuard();

            if (output && !output.isValid()) // if output was passed, and it's invalid => error
            {
                pushError(Result::InvalidHandle, "Engine::Impl::createBus failed because output Bus was invalid");
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
                Bus::connect(outputBus, static_cast<Handle<Source>>(newBusHandle));

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
                pushError(Result::InvalidHandle, "Engine::releaseBus: bus was invalid");
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
            auto lockGuard = m_device->mixLockGuard();
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
            auto lockGuard = m_device->mixLockGuard();
            ENGINE_INIT_GUARD();

            if (outSpec)
            {
                *outSpec = m_device->spec();
            }

            return true;
        }

        bool getMasterBus(Handle<Bus> *outBus) const
        {
            auto lockGuard = m_device->mixLockGuard();
            ENGINE_INIT_GUARD();

            if (outBus)
            {
                *outBus = m_masterBus;
            }

            return true;
        }

        bool getPaused(bool *outValue) const
        {
            auto lockGuard = m_device->mixLockGuard();
            ENGINE_INIT_GUARD();

            if (outValue)
            {
                *outValue = m_device->isRunning();
            }

            return true;
        }

        bool setPaused(const bool value)
        {
            ENGINE_INIT_GUARD();
            auto lockGuard = m_device->mixLockGuard();

            if (value)
                m_device->suspend();
            else
                m_device->resume();
            return true;
        }

        bool update()
        {
            ENGINE_INIT_GUARD();
            auto lockGuard = m_device->mixLockGuard();
            {
                auto deferredCommandGuard = std::lock_guard(m_deferredCommandMutex);
                processCommands(this, m_deferredCommands);
            }


            if (m_discardFlag)
            {
                if (m_masterBus.isValid()) // is this necessary?
                {
                    m_masterBus->processRemovals();
                }
                else
                {
                    pushError(Result::InvalidHandle, "Internal error: master bus is invalidated");
                    return false;
                }

                m_discardFlag = false;
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
            if (!engine->isOpen())
                return;

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

    bool Engine::releaseSoundImpl(const Handle<Source> &source)
    {
        return m->release(source, false);
    }

    bool Engine::releaseSoundRaw(Source *source, const bool recursive)
    {
        return m->releaseRaw(source, recursive);
    }

    bool Engine::releaseBus(const Handle<Bus> &bus, const bool recursive)
    {
        return m->release(static_cast<Handle<Source>>(bus), recursive);
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
}
