#include "Engine.h"

#include "AudioSpec.h"
#include "Bus.h"
#include "Error.h"
#include "PCMSource.h"
#include "SoundBuffer.h"

#include <SDL2/SDL_audio.h>

#include <iostream>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "SourceHandle.h"
#include "private/SdlAudioGuard.h"

static int getDefaultSampleRate();

namespace insound {
    struct Engine::Impl {
    public:
        explicit Impl(Engine *engine) : m_engine(engine), m_spec(), m_deviceID(), m_clock(), m_masterBus(),
                                        m_mixMutex(),
                                        m_deferredCommands(), m_immediateCommands(), m_bufferSize(0),
                                        m_discardFlag(false), m_initGuard(),
                                        m_sources()
        {
        }

        ~Impl()
        {
            close();
        }

        bool open()
        {
            SDL_AudioSpec desired, obtained;

            SDL_memset(&desired, 0, sizeof(desired));
            desired.channels = 2;
            desired.format = AUDIO_F32;
            desired.freq = getDefaultSampleRate();
            desired.userdata = this;
            desired.callback = audioCallback;

            const auto tempDeviceID = SDL_OpenAudioDevice(nullptr, false, &desired, &obtained, 0);
            if (tempDeviceID == 0)
            {
                pushError(Error::RuntimeErr, std::string("Failed to open audio device: ") + SDL_GetError());
                return false;
            }

            m_spec.channels = obtained.channels;
            m_spec.freq = obtained.freq;
            m_spec.format = SampleFormat(
                SDL_AUDIO_BITSIZE(obtained.format), SDL_AUDIO_ISFLOAT(obtained.format),
                SDL_AUDIO_ISBIGENDIAN(obtained.format), SDL_AUDIO_ISSIGNED(obtained.format));

            m_deviceID = tempDeviceID;
            m_clock = 0;
            m_masterBus = new Bus(m_engine, nullptr, false);
            m_bufferSize = obtained.size;

            SDL_PauseAudioDevice(tempDeviceID, SDL_FALSE);
            return true;
        }

        void close()
        {
            if (isOpen())
            {
                if (m_masterBus)
                {
                    m_masterBus->release(true);
                    processCommands(m_immediateCommands); // flush command buffers
                    processCommands(m_deferredCommands);
                    m_masterBus->processRemovals();
                    delete m_masterBus;
                    m_masterBus = nullptr;
                }

                SDL_CloseAudioDevice(m_deviceID);
                m_deviceID = 0;
                m_clock = 0;
            }
        }

        void flagDiscard(SoundSource *source)
        {
            if (m_sources.erase(source))
            {
                m_discardFlag = true;
            }
        }

        [[nodiscard]]
        bool getBufferSize(uint32_t *outSize) const
        {
            if (!isOpen())
            {
                pushError(Error::LogicErr, "Couldn't retrieve buffer size: audio device was not open");
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

        SourceHandle<PCMSource> playSound(const SoundBuffer *buffer, bool paused, bool looping, bool oneshot, Bus *bus)
        {
            auto newSource = new PCMSource(m_engine, buffer, bus ? bus->clock() : m_masterBus->clock(), paused, looping, oneshot);

            std::lock_guard lockGuard(m_mixMutex); // we don't want to append while processing is occuring
            if (bus != nullptr)
            {
                bus->appendSource(newSource);
            }
            else
            {
                m_masterBus->appendSource(newSource);
            }

            m_sources.emplace(newSource);

            return SourceHandle(newSource, m_engine);
        }

        SourceHandle<Bus> createBus(bool paused, Bus *output)
        {
            auto newBus = new Bus(m_engine, output ? output : m_masterBus, paused);

            m_sources.emplace(newBus);

            return SourceHandle(newBus, m_engine);
        }

        [[nodiscard]]
        bool isSourceValid(SoundSource *source) const
        {
            const auto it = m_sources.find(source);
            return it != m_sources.end();
        }

        /// non-zero positive number if open, zero if not
        [[nodiscard]]
        uint32_t deviceID() const
        {
            return m_deviceID;
        }

        /// Get audio specification data
        /// @param outSpec spec to receive
        /// @returns true on success, false on error
        bool getSpec(AudioSpec *outSpec) const
        {
            if (!isOpen())
            {
                pushError(Error::LogicErr, "Couldn't retrieve audio spec: audio device was not open");
                return false;
            }

            if (outSpec)
            {
                *outSpec = m_spec;
            }

            return true;
        }

        bool getMasterBus(Bus **outBus) const
        {
            if (!isOpen())
            {
                printf("Couldn't retrieve master bus: audio device was not open\n");
                return false;
            }

            if (outBus)
            {
                *outBus = m_masterBus;
            }

            return true;
        }

        void update()
        {
            std::lock_guard lockGuard(m_mixMutex);
            processCommands(m_deferredCommands);

            if (m_discardFlag)
            {
                m_masterBus->processRemovals();
                m_discardFlag = false;
            }
        }

        void pushCommand(const Command &command)
        {
            std::lock_guard lockGuard(m_mixMutex);
            m_deferredCommands.emplace_back(command);
        }

        void pushImmediateCommand(const Command &command)
        {
            std::lock_guard lockGuard(m_mixMutex);
            m_immediateCommands.emplace_back(command);
        }

    private:
        static void processCommands(std::vector<Command> &commands)
        {
            if (commands.empty())
                return;

            for (auto &command : commands)
            {
                switch(command.type)
                {
                    case Command::EffectParamSet:
                    {
                        command.data.effect.effect->receiveParam(
                            command.data.effect.index, command.data.effect.value);
                    } break;

                    case Command::SoundSource:
                    {
                        command.data.source.source->applyCommand(command);
                    } break;

                    case Command::PCMSource:
                    {
                        command.data.pcmsource.source->applyPCMSourceCommand(command);
                    } break;

                    case Command::Bus:
                    {
                        command.data.bus.bus->applyBusCommand(command);
                    } break;

                    default:
                    {

                    } break;
                }
            }
            commands.clear();
        }

        static void audioCallback(void *userptr, uint8_t *stream, int length)
        {
            auto engine = (Engine::Impl *)userptr;

            std::lock_guard lockGuard(engine->m_mixMutex);


            Engine::Impl::processCommands(engine->m_immediateCommands); // process any commands that

            const uint8_t *mixPtr;
            const auto bytesRead = engine->m_masterBus->read(&mixPtr, length);
            engine->m_masterBus->updateParentClock(engine->m_clock);

            // set stream to zero
            std::memset(stream, 0, length);

            // copy mix into result
            std::memcpy(stream, mixPtr, bytesRead);

            engine->m_clock += length / (2 * sizeof(float));
        }

        Engine *m_engine;
        AudioSpec m_spec;
        SDL_AudioDeviceID m_deviceID;
        uint32_t m_clock;

        Bus *m_masterBus;
        std::mutex m_mixMutex;
        std::vector<Command> m_deferredCommands;
        std::vector<Command> m_immediateCommands;
        uint32_t m_bufferSize;
        bool m_discardFlag; ///< set to true when sound source discard should be made
        detail::SdlAudioGuard m_initGuard;

        std::unordered_set<SoundSource *> m_sources;
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

    /// TODO: make this return bool, and retrieve value via outval
    SourceHandle<PCMSource> Engine::playSound(const SoundBuffer *buffer, bool paused, bool looping, bool oneshot, SourceHandle<Bus> bus)
    {
        return m->playSound(buffer, paused, looping, oneshot, bus.get());
    }

    /// TODO: make this return bool, and retrieve value via outval
    SourceHandle<Bus> Engine::createBus(bool paused, Bus *output)
    {
        return m->createBus(paused, output);
    }

    uint32_t Engine::deviceID() const
    {
        return m->deviceID();
    }

    bool Engine::getSpec(AudioSpec *outSpec) const
    {
        return m->getSpec(outSpec);
    }

    bool Engine::getBufferSize(uint32_t *outSize) const
    {
        return m->getBufferSize(outSize);
    }

    bool Engine::getMasterBus(Bus **bus) const
    {
        return m->getMasterBus(bus);
    }

    void Engine::pushCommand(const Command &command)
    {
        m->pushCommand(command);
    }

    void Engine::pushImmediateCommand(const Command &command)
    {
        m->pushImmediateCommand(command);
    }

    void Engine::update()
    {
        m->update();
    }

    void Engine::flagDiscard(SoundSource *source)
    {
        m->flagDiscard(source);
    }

    bool Engine::isSourceValid(SoundSource *source)
    {
        return m->isSourceValid(source);
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
        printf("Failed to get default device specs: %s\n", SDL_GetError());
        return false;
    }

    return defaultSpec.freq;
}
#endif