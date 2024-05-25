#include "Engine.h"
#include "SoundBuffer.h"
#include "Bus.h"
#include "PCMSource.h"

#include <SDL2/SDL_audio.h>

#include <iostream>
#include <mutex>
#include <vector>

static int getDefaultSampleRate();

namespace insound {
    struct Engine::Impl {
    public:
        Impl(Engine *engine) : m_engine(engine), m_spec(), m_deviceID(), m_clock(), m_masterBus(), m_mixMutex(), m_commands()
        { }

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
                printf("Failed to open audio device: %s\n", SDL_GetError());
                return false;
            }

            m_spec = obtained;
            m_deviceID = tempDeviceID;
            m_clock = 0;
            m_masterBus = new Bus(m_engine, nullptr);

            SDL_PauseAudioDevice(tempDeviceID, SDL_FALSE);
            return true;
        }

        void close()
        {
            if (isOpen())
            {
                SDL_CloseAudioDevice(m_deviceID);
                m_deviceID = 0;
                m_clock = 0;

                if (m_masterBus)
                {
                    delete m_masterBus;
                    m_masterBus = nullptr;
                }

            }
        }

        [[nodiscard]]
        bool isOpen() const
        {
            return m_deviceID != 0;
        }

        PCMSource *playSound(const SoundBuffer *buffer, bool paused, Bus *bus)
        {
            auto newSource = new PCMSource(m_engine, buffer, bus ? bus->clock() : m_masterBus->clock(), paused);

            std::lock_guard lockGuard(m_mixMutex); // we don't want to append while processing is occuring
            if (bus != nullptr)
            {
                bus->appendSource(newSource);
            }
            else
            {
                m_masterBus->appendSource(newSource);
            }

            return newSource;
        }

        [[nodiscard]]
        uint32_t deviceID() const
        {
            return m_deviceID;
        }

        bool getSpec(SDL_AudioSpec *outSpec) const
        {
            if (!isOpen())
            {
                printf("Couldn't retrieve audio spec: audio device was not open\n");
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
            processCommands();
        }

        void pushCommand(const Command &command)
        {
            std::lock_guard lockGuard(m_mixMutex);
            m_commands.emplace_back(command);
        }

    private:
        void processCommands()
        {
            if (m_commands.empty())
                return;

            for (auto &command : m_commands)
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

                    default:
                    {

                    } break;
                }
            }
            m_commands.clear();
        }

        static void audioCallback(void *userptr, uint8_t *stream, int length)
        {
            auto engine = (Engine::Impl *)userptr;
            std::lock_guard lockGuard(engine->m_mixMutex);

            const uint8_t *mixPtr;
            const auto bytesRead = engine->m_masterBus->read(&mixPtr, length, 0);

            // set stream to zero
            std::memset(stream, 0, length);

            // copy mix into result
            std::memcpy(stream, mixPtr, bytesRead);

            engine->m_clock += length;
        }

        Engine *m_engine;
        SDL_AudioSpec m_spec;
        SDL_AudioDeviceID m_deviceID;
        uint32_t m_clock;

        Bus *m_masterBus;
        std::mutex m_mixMutex;
        std::vector<Command> m_commands;
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

    PCMSource *Engine::playSound(const SoundBuffer *buffer, bool paused, Bus *bus)
    {
        return m->playSound(buffer, paused, bus);
    }

    uint32_t Engine::deviceID() const
    {
        return m->deviceID();
    }

    bool Engine::getSpec(SDL_AudioSpec *outSpec) const
    {
        return m->getSpec(outSpec);
    }

    bool Engine::getMasterBus(Bus **bus)
    {
        return m->getMasterBus(bus);
    }

    void Engine::pushCommand(const Command &command)
    {
        m->pushCommand(command);
    }

    void Engine::update()
    {
        m->update();
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