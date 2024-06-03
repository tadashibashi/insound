#include "Engine.h"

#include "AudioSpec.h"
#include "Bus.h"
#include "Command.h"
#include "Effect.h"
#include "Error.h"
#include "PCMSource.h"
#include "SoundBuffer.h"
#include "SourceRef.h"

#include "private/SdlAudioGuard.h"

#include <SDL2/SDL_audio.h>

#include <iostream>
#include <mutex>
#include <unordered_set>
#include <vector>

#ifdef INSOUND_THREADING
#include <thread>
#endif

static int getDefaultSampleRate();

namespace insound {
    struct Engine::Impl {
    public:
        explicit Impl(Engine *engine) : m_engine(engine), m_spec(), m_deviceID(), m_clock(), m_masterBus(),
                                        m_mixMutex(),
                                        m_deferredCommands(), m_immediateCommands(), m_bufferSize(0), m_threadDelay(),
                                        m_discardFlag(false), m_initGuard(),
                                        m_sources(), m_buffer(), m_bufferReadyToQueue(false), m_bufferReadyToSwap(false)
        {
        }

        ~Impl()
        {
            close();
        }

        bool open(const int frequency = 0, const int samples = 1024)
        {
            SDL_AudioSpec desired, obtained;

            SDL_memset(&desired, 0, sizeof(desired));
            desired.channels = 2;
            desired.format = AUDIO_F32;
            desired.freq = frequency ? frequency : getDefaultSampleRate();
            desired.samples = samples;
#ifndef INSOUND_THREADING
            desired.callback = audioCallback;
            desired.userdata = this;
#endif

            const auto tempDeviceID = SDL_OpenAudioDevice(nullptr, false, &desired, &obtained, 0);
            if (tempDeviceID == 0)
            {
                pushError(Result::SdlErr, SDL_GetError());
                return false;
            }

            m_spec.channels = obtained.channels;
            m_spec.freq = obtained.freq;
            m_spec.format = SampleFormat(
                SDL_AUDIO_BITSIZE(obtained.format), SDL_AUDIO_ISFLOAT(obtained.format),
                SDL_AUDIO_ISBIGENDIAN(obtained.format), SDL_AUDIO_ISSIGNED(obtained.format));

            m_deviceID = tempDeviceID;
            m_clock = 0;
            m_masterBus = Bus::createMasterBus(m_engine);

            m_bufferSize = samples * sizeof(float) * 2;
            m_threadDelay = std::chrono::microseconds((int)((float)samples / (float)obtained.freq * 500000.f) );
            std::cout << "thread delay: " << m_threadDelay.count() << '\n';
            m_sources.emplace(m_masterBus.get(), m_masterBus);

            SDL_PauseAudioDevice(tempDeviceID, SDL_FALSE);

#ifdef INSOUND_THREADING
            m_audioThread = std::thread([this]() {
                queueAudio();
            });

            m_mixThread = std::thread([this]() {
                mixCallback();
            });
#endif

            return true;
        }

        void close()
        {

            if (isOpen())
            {
                auto deviceID = m_deviceID;
                {
                    std::lock_guard lockGuard(m_mixMutex); // this should safely end the mix / audio queue threads
                    std::lock_guard lockGuard2(m_audioQueueMutex);
                    if (m_masterBus.get() && isSourceValid(m_masterBus.get()))
                    {
                        processCommands(this, m_immediateCommands); // flush command buffers
                        processCommands(this, m_deferredCommands);
                        Bus::releaseMasterBus(m_masterBus.get());
                        m_masterBus->release(true);
                        m_masterBus->processRemovals();
                        m_masterBus.reset();
                    }

                    m_sources.clear();
                    m_deviceID = 0;
                }

#ifdef INSOUND_THREADING
                m_mixThread.join();
                m_audioThread.join();
#endif
                SDL_CloseAudioDevice(deviceID);
                m_clock = 0;
            }
        }

        /// @param source source to flag discard
        bool flagDiscard(SoundSource *source)
        {
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            pushCommand(Command::makeEngineReleaseSource(m_engine, source));

            return true;
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
                *outSize = m_bufferSize;

            return true;
        }

        [[nodiscard]]
        bool isOpen() const
        {
            return m_deviceID != 0;
        }

        bool playSound(const SoundBuffer *buffer, bool paused, bool looping, bool oneshot, Bus *bus, PCMSourceRef *outPcmSource)
        {
            std::lock_guard lockGuard(m_mixMutex); // we don't want to append while processing is occuring

            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            if (bus && !isSourceValid(bus)) // if output bus was passed, and it's invalid => error
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

            auto newSource = std::make_shared<PCMSource>(m_engine, buffer, clock, paused, looping, oneshot);

            if (bus != nullptr)
            {
                bus->applyAppendSource(newSource.get());
            }
            else
            {
                m_masterBus->applyAppendSource(newSource.get());
            }

            m_sources.emplace(newSource.get(), newSource);

            if (outPcmSource)
                *outPcmSource = SourceRef(newSource, m_engine);

            return true;
        }

        bool createBus(bool paused, Bus *output, BusRef *outBus)
        {
            std::lock_guard lockGuard(m_mixMutex);
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            if (output && !isSourceValid(output)) // if output was passed, and it's invalid => error
            {
                pushError(Result::InvalidHandle, "Engine::Impl::createBus failed because output Bus was invalid");
                return false;
            }

            auto newBus = std::make_shared<Bus>(m_engine, output ? output : m_masterBus.get(), paused);

            m_sources.emplace(newBus.get(), newBus);

            if (outBus)
                *outBus = SourceRef(newBus, m_engine);
            return true;
        }

        [[nodiscard]]
        bool isSourceValid(const SoundSource *source) const
        {
            std::lock_guard lockGuard(m_mixMutex);
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            return m_sources.find(source) != m_sources.end();
        }

        /// Non-zero positive number if open, zero if not
        [[nodiscard]]
        bool deviceID(uint32_t *outDeviceID) const
        {
            std::lock_guard lockGuard(m_mixMutex);
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            if (outDeviceID)
                *outDeviceID = m_deviceID;
            return true;
        }

        /// Get audio specification data
        /// @param outSpec spec to receive
        /// @returns true on success, false on error
        bool getSpec(AudioSpec *outSpec) const
        {
            std::lock_guard lockGuard(m_mixMutex);
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            if (outSpec)
            {
                *outSpec = m_spec;
            }

            return true;
        }

        bool getMasterBus(std::shared_ptr<Bus> *outBus) const
        {
            std::lock_guard lockGuard(m_mixMutex);
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
                auto state = SDL_GetAudioDeviceStatus(m_deviceID);

                *outValue = state == SDL_AUDIO_PAUSED;
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

            SDL_PauseAudioDevice(m_deviceID, value);
            return true;
        }

        bool update()
        {
            std::lock_guard lockGuard(m_mixMutex);
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            processCommands(this, m_deferredCommands);

            if (m_discardFlag)
            {
                if (isSourceValid(m_masterBus.get())) // is this necessary?
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
            std::lock_guard lockGuard(m_mixMutex);
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
            std::lock_guard lockGuard(m_mixMutex);
            if (!isOpen())
            {
                pushError(Result::EngineNotInit);
                return false;
            }

            m_immediateCommands.emplace_back(command);

            return true;
        }


        void processCommand(const EngineCommand &command)
        {
            // This is called from the mix thread, so we don't need to lock

            switch(command.type)
            {
                case EngineCommand::ReleaseSource:
                {
                    const auto source = command.releasesource.source;

                    auto it = m_sources.find(source);
                    if (it != m_sources.end())
                    {
                        source->m_shouldDiscard = true;
                        m_sources.erase(it);
                        m_discardFlag = true;
                    }
                } break;

                default:
                {
                    // unknown engine command
                } break;
            }
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

                    case Command::SoundSource:
                    {
                        if (engine->isSourceValid(command.source.source))
                            command.source.source->applyCommand(command.source);
                    } break;

                    case Command::PCMSource:
                    {
                        if (engine->isSourceValid(command.pcmsource.source))
                            command.pcmsource.source->applyCommand(command.pcmsource);
                    } break;

                    case Command::Bus:
                    {
                        if (engine->isSourceValid(command.bus.bus))
                            command.bus.bus->applyCommand(command.bus);
                    } break;

                    default:
                    {

                    } break;
                }
            }
            commands.clear();
        }

#ifdef INSOUND_THREADING
        void queueAudio()
        {
            while(true)
            {
                const auto startTime = std::chrono::high_resolution_clock::now();
                {
                    std::lock_guard lockGuard(m_audioQueueMutex);
                    if (m_deviceID == 0)
                    {
                        break;
                    }

                    if (const auto bufferSize = m_buffer.size();
                        m_bufferReadyToQueue && (int)SDL_GetQueuedAudioSize(m_deviceID) - (int)bufferSize <= 0)
                    {
                        if (SDL_QueueAudio(m_deviceID, m_buffer.data(), bufferSize) != 0)
                        {
                            detail::pushSystemError(Result::SdlErr, SDL_GetError());
                            break; // invalid device or some problem
                        }
                        else
                        {
                            m_bufferReadyToQueue = false;
                        }
                    }
                }
                const auto endTime = std::chrono::high_resolution_clock::now();
                std::this_thread::sleep_until(m_threadDelay - (endTime - startTime) + endTime);
            }
        }

        void mixCallback()
        {
            while(true)
            {
                const auto startTime = std::chrono::high_resolution_clock::now();

                if (m_deviceID == 0)
                    break;

                if (const auto status = SDL_GetAudioDeviceStatus(m_deviceID);
                    status == SDL_AUDIO_PLAYING)
                {
                    std::lock_guard lockGuard(m_mixMutex);
                    if (!m_bufferReadyToSwap)
                    {
                        // Process commands that require sample accurate immediacy
                        Engine::Impl::processCommands(this, m_immediateCommands);

                        // Fill masterbus' buffer with updated data
                        const auto bytesRead = m_masterBus->read(nullptr, m_bufferSize);

                        m_masterBus->updateParentClock(m_clock);
                        m_clock += m_bufferSize / (2 * sizeof(float));

                        m_bufferReadyToSwap = true;
                    }

                    std::lock_guard lockGuard2(m_audioQueueMutex);
                    if (m_bufferReadyToSwap && !m_bufferReadyToQueue)
                    {

                        m_masterBus->swapBuffers(&m_buffer);

                        m_bufferReadyToQueue = true;
                        m_bufferReadyToSwap = false;
                    }
                }
                const auto endTime = std::chrono::high_resolution_clock::now();
                std::this_thread::sleep_until(m_threadDelay - (endTime - startTime) + endTime);
            }
        }
        std::thread m_audioThread;
        std::thread m_mixThread;
#else
        // To be used for emscripten and non-pthread environments
        static void audioCallback(void *userptr, uint8_t *stream, int length)
        {
            auto engine = (Engine::Impl *)userptr;

            std::lock_guard lockGuard(engine->m_mixMutex);

            Engine::Impl::processCommands(engine, engine->m_immediateCommands); // process any commands that

            const uint8_t *mixPtr;
            const auto bytesRead = engine->m_masterBus->read(&mixPtr, length);
            engine->m_masterBus->updateParentClock(engine->m_clock);

            // set stream to zero
            std::memset(stream, 0, length);

            // copy mix into result
            std::memcpy(stream, mixPtr, bytesRead);

            engine->m_clock += length / (2 * sizeof(float));
        }
#endif
        Engine *m_engine;
        AudioSpec m_spec;
        SDL_AudioDeviceID m_deviceID;
        uint32_t m_clock;

        std::shared_ptr<Bus> m_masterBus;

        mutable std::recursive_mutex m_mixMutex;
        mutable std::recursive_mutex m_audioQueueMutex;
        std::vector<Command> m_deferredCommands;
        std::vector<Command> m_immediateCommands;
        uint32_t m_bufferSize;
        std::chrono::microseconds m_threadDelay;

        bool m_discardFlag; ///< set to true when sound source discard should be made
        detail::SdlAudioGuard m_initGuard;

        std::unordered_map<const SoundSource *, std::shared_ptr<SoundSource>> m_sources;
        std::vector<uint8_t> m_buffer; ///< buffer to read from

        bool m_bufferReadyToQueue;
        bool m_bufferReadyToSwap;
    };

    Engine::Engine() : m(new Impl(this))
    { }

    Engine::~Engine()
    {
        delete m;
    }

    bool Engine::open()
    {
        return m->open();
    }

    void Engine::close()
    {
        m->close();
    }

    bool Engine::isOpen() const
    {
        return m->isOpen();
    }

    bool Engine::playSound(const SoundBuffer *buffer, bool paused, bool looping, bool oneshot, BusRef &bus, PCMSourceRef *outPcmSource)
    {
        return m->playSound(buffer, paused, looping, oneshot, bus.get(), outPcmSource);
    }

    bool Engine::playSound(const SoundBuffer *buffer, bool paused, bool looping, bool oneshot, PCMSourceRef *outPcmSource)
    {
        return m->playSound(buffer, paused, looping, oneshot, nullptr, outPcmSource);
    }

    /// TODO: make this return bool, and retrieve value via outval
    bool Engine::createBus(bool paused, SourceRef<Bus> &output, SourceRef<Bus> *outBus)
    {
        return m->createBus(paused, output.get(), outBus);
    }

    bool Engine::createBus(bool paused, SourceRef<Bus> *outBus)
    {
        return m->createBus(paused, nullptr, outBus);
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

    bool Engine::getMasterBus(SourceRef<Bus> *outbus) const
    {
        std::shared_ptr<Bus> masterBus;
        auto result = m->getMasterBus(&masterBus);

        *outbus = SourceRef(masterBus, this);
        return result;
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

    bool Engine::flagDiscard(SoundSource *source)
    {
        return m->flagDiscard(source);
    }

    bool Engine::isSourceValid(const std::shared_ptr<SoundSource> &source) const
    {
        return m->isSourceValid(source.get());
    }

    bool Engine::isSourceValid(const SoundSource *source) const
    {
        return m->isSourceValid(source);
    }

    void Engine::applyCommand(const EngineCommand &command)
    {
        m->processCommand(command);
    }
}

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
int getDefaultSampleRate() {
    return EM_ASM_INT({
        var AudioContext = window.AudioContext || window.webkitAudioContext;
        var ctx = new AudioContext();
        var sr = ctx.sampleRate;
        ctx.close();
        return sr;
    });
}
#else
int getDefaultSampleRate()
{
    SDL_AudioSpec defaultSpec;
    if (SDL_GetDefaultAudioInfo(nullptr, &defaultSpec, SDL_FALSE) != 0)
    {
        insound::pushError(insound::Result::SdlErr, SDL_GetError());
        return 0;
    }

    return defaultSpec.freq;
}
#endif
