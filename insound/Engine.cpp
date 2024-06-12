#include "Engine.h"

#include "AudioSpec.h"
#include "Bus.h"
#include "Command.h"
#include "Effect.h"
#include "Error.h"
#include "PCMSource.h"

#include <mutex>
#include <unordered_map>
#include <vector>

#include <insound/platform/AudioDevice/AudioDevice.h>


namespace insound {
    struct Engine::Impl {
    public:
        explicit Impl(Engine *engine) : m_engine(engine), m_clock(), m_masterBus(),
                                        m_device(), m_deferredCommands(),
                                        m_immediateCommands(),
                                        m_discardFlag(false)
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
                    m_masterBus->m_isMaster = false;
                    release((Handle<Source>)m_masterBus, true);

                    processCommands(this, m_immediateCommands); // flush command buffers
                    processCommands(this, m_deferredCommands);

                    m_masterBus->processRemovals();
                    m_engine->destroySource((Handle<Source>)m_masterBus);
                    m_masterBus = {};
                }

                m_clock = 0;
                m_device->close();
            }
        }

        [[nodiscard]]
        bool getBufferSize(uint32_t *outSize) const
        {
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

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
        bool playSound(const SoundBuffer *buffer, bool paused, bool looping, bool oneshot, Handle<Bus> bus, Handle<PCMSource> *outPcmSource)
        {
            auto lockGuard = m_device->mixLockGuard();

            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

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

            auto newSource = m_sources.allocate<PCMSource>(m_engine, buffer, clock, paused, looping, oneshot);

            if (bus)
            {
                bus->applyAppendSource((Handle<Source>)newSource);
            }
            else
            {
                m_masterBus->applyAppendSource((Handle<Source>)newSource);
            }

            if (outPcmSource)
                *outPcmSource = newSource;

            m_aliveSources.emplace(newSource.id().id, newSource);
            return true;
        }

        bool createBus(bool paused, const Handle<Bus> &output, Handle<Bus> *outBus, bool isMaster)
        {
            auto lockGuard = m_device->mixLockGuard();
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            if (output && !output.isValid()) // if output was passed, and it's invalid => error
            {
                pushError(Result::InvalidHandle, "Engine::Impl::createBus failed because output Bus was invalid");
                return false;
            }

            auto outputBus =  isMaster ? Handle<Bus>{} : output.isValid() ? output : m_masterBus;
            auto newBusHandle = m_sources.allocate<Bus>(
                m_engine,
                outputBus,
                paused);
            Bus::setOutput(newBusHandle, outputBus);

            if (isMaster)
                newBusHandle->m_isMaster = true;

            if (outBus)
            {
                *outBus = newBusHandle;
            }

            m_aliveSources.emplace(newBusHandle.id().id, newBusHandle);
            return true;
        }

        bool release(Handle<Source> source, bool recursive)
        {
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            if (!source.isValid())
            {
                pushError(Result::InvalidHandle, "Engine::releaseBus: bus was invalid");
                return false;
            }

            m_discardFlag = true;
            m_aliveSources.erase(source.id().id);
            return m_engine->pushCommand(Command::makeEngineDeallocateSource(m_engine, source, recursive));
        }

        /// Non-zero positive number if open, zero if not
        [[nodiscard]]
        bool deviceID(uint32_t *outDeviceID) const
        {
            auto lockGuard = m_device->mixLockGuard();
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

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
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            if (outSpec)
            {
                *outSpec = m_device->spec();
            }

            return true;
        }

        bool getMasterBus(Handle<Bus> *outBus) const
        {
            auto lockGuard = m_device->mixLockGuard();
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            if (outBus)
            {
                *outBus = m_masterBus;
            }

            return true;
        }

        bool getPaused(bool *outValue) const
        {
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            if (outValue)
            {
                *outValue = m_device->isRunning();
            }

            return true;
        }

        bool setPaused(const bool value)
        {
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            if (value)
                m_device->suspend();
            else
                m_device->resume();
            return true;
        }

        bool update()
        {;
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            auto lockGuard = m_device->mixLockGuard();
            processCommands(this, m_deferredCommands);

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
            auto lockGuard = m_device->mixLockGuard();
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            m_deferredCommands.emplace_back(command);
            return true;
        }

        bool pushImmediateCommand(const Command &command)
        {
            auto lockGuard = m_device->mixLockGuard();
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            m_immediateCommands.emplace_back(command);

            return true;
        }


        void processCommand(EngineCommand &command)
        {
            // This is called from the mix thread, so we don't need to lock

            switch(command.type)
            {
                case EngineCommand::ReleaseSource:
                {
                    auto &source = command.deallocsource.source;
                    if (!source)
                        break;

                    if (auto bus = source.getAs<Bus>())
                    {
                        bus->release(command.deallocsource.recursive);
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
        const SourcePool &getSourcePools() const
        {
            return m_sources;
        }

        SourcePool &getSourcePools()
        {
            return m_sources;
        }
    private:
        static void processCommands(const Engine::Impl *engine, std::vector<Command> &commands)
        {
            if (commands.empty())
                return;

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

        static void audioCallback(void *userptr, std::vector<uint8_t> *outBuffer)
        {
            const auto engine = (Engine::Impl *)userptr;
            if (!engine->isOpen())
                return;

            // Process commands that require sample-accurate immediacy
            Engine::Impl::processCommands(engine, engine->m_immediateCommands);
            const auto size = outBuffer->size();
            engine->m_masterBus->read(nullptr, (int)size);
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
        SourcePool m_sources; ///< manages multiple pools of different source types

        std::unordered_map<size_t, Handle<Source>> m_aliveSources;
    };

    Engine::Engine() : m(new Impl(this))
    { }

    Engine::~Engine()
    {
        delete m;
    }

    bool Engine::open(int samplerate, int bufferFrameSize)
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

    bool Engine::playSound(const SoundBuffer *buffer, bool paused, bool looping, bool oneshot, Handle<Bus> bus, Handle<PCMSource> *outPcmSource)
    {
        return m->playSound(buffer, paused, looping, oneshot, bus, outPcmSource);
    }

    bool Engine::playSound(const SoundBuffer *buffer, bool paused, bool looping, bool oneshot, Handle<PCMSource> *outPcmSource)
    {
        return m->playSound(buffer, paused, looping, oneshot, {}, outPcmSource);
    }

    bool Engine::createBus(bool paused, Handle<Bus> output, Handle<Bus> *outBus)
    {
        return m->createBus(paused, output, outBus, false);
    }

    bool Engine::createBus(bool paused, Handle<Bus> *outBus)
    {
        return m->createBus(paused, {}, outBus, false);
    }

    bool Engine::releaseSoundImpl(Handle<Source> source) // TODO: move this to engine impl
    {
        return m->release(source, false);
    }

    bool Engine::releaseBus(Handle<Bus> bus, bool recursive) // TODO: move this to engine impl
    {
        return m->release((Handle<Source>)bus, recursive);
    }

    bool Engine::deviceID(uint32_t *outDeviceID) const
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

    bool Engine::setPaused(bool value)
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


    void Engine::destroySource(Handle<Source> source)
    {
        m->getSourcePools().deallocate(source);
    }

    const SourcePool &Engine::getSourcePool() const
    {
        return m->getSourcePools();
    }

    SourcePool &Engine::getSourcePool()
    {
        return m->getSourcePools();
    }

    void Engine::applyCommand(EngineCommand &command)
    {
        m->processCommand(command);
    }
}
