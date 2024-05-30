
#include <insound/Engine.h>
#include <insound/SoundBuffer.h>
#include <insound/PCMSource.h>
#include <insound/Bus.h>

#include <SDL2/SDL.h>

#include <iostream>

#include "insound/AudioSpec.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <functional>

std::function<void()> emMainLoopCallback;
void emMainLoop()
{
    emMainLoopCallback();
}
#endif

using namespace insound;

int main()
{
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        printf("Failed to init SDL2 %s\n", SDL_GetError());
        return -1;
    }

    auto window = SDL_CreateWindow("Mixer test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        400, 400, 0);
    if (!window)
    {
        printf("SDL window failed to create: %s\n", SDL_GetError());
        return -1;
    }

    Engine engine;
    if (!engine.open())
    {
        printf("Failed to init sound engine\n");
        return -1;
    }

    AudioSpec spec;
    engine.getSpec(&spec);

    SoundBuffer sounds[4];
    sounds[0].load("assets/bassdrum.wav", spec);
    sounds[1].load("assets/ep.wav", spec);
    sounds[2].load("assets/piano.wav", spec);
    sounds[3].load("assets/snare-hat.wav", spec);

    PCMSource *sources[4]; // using PCMSource ptrs here is error prone, because engine will delete ones that end
    sources[0] = engine.playSound(&sounds[0], false);
    sources[1] = engine.playSound(&sounds[1], false);
    sources[2] = engine.playSound(&sounds[2], false);
    sources[3] = engine.playSound(&sounds[3], false);

    // for (auto source : sources)
    // {
    //     source->paused(false);
    // }

    Bus *masterBus;
    engine.getMasterBus(&masterBus);

    bool isRunning = true;
    int channelSelect = 0;

    bool paused = false;

#ifdef __EMSCRIPTEN__
    emMainLoopCallback = [&]() {
#else
    while (isRunning)
#endif
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            switch(e.type)
            {
                case SDL_QUIT:
                {
                    isRunning = false;
                } break;

                case SDL_KEYUP:
                {
                    switch(e.key.keysym.scancode)
                    {
                        case SDL_SCANCODE_Q:
                        {
                                isRunning = false;
#ifdef __EMSCRIPTEN__
                                emscripten_force_exit(0);
#endif
                        } break;
                        case SDL_SCANCODE_1:
                        {
                            channelSelect = 0;
                        } break;

                        case SDL_SCANCODE_2:
                        {
                            channelSelect = 1;
                        } break;

                        case SDL_SCANCODE_3:
                        {
                            channelSelect = 2;
                        } break;

                        case SDL_SCANCODE_4:
                        {
                            channelSelect = 3;
                        } break;

                        case SDL_SCANCODE_P:
                        {
                            std::cout << "Send pause command at " << masterBus->parentClock() << '\n';
                            masterBus->paused(!paused, masterBus->parentClock() + spec.freq);
                            paused = !paused;
                        } break;

                        case SDL_SCANCODE_Z:
                        {
                            masterBus->panner()->left(masterBus->panner()->left() + .05f);
                        } break;
                        case SDL_SCANCODE_X:
                        {
                            masterBus->panner()->left(masterBus->panner()->left() - .05f);
                        } break;
                        case SDL_SCANCODE_C:
                        {
                            masterBus->panner()->right(masterBus->panner()->right() - .05f);
                        } break;
                        case SDL_SCANCODE_V:
                        {
                            masterBus->panner()->right(masterBus->panner()->right() + .05f);
                        } break;

                        case SDL_SCANCODE_R:
                        {
                            // TODO: restart track logic
                        } break;

                        case SDL_SCANCODE_UP:
                        {
                            auto source = sources[channelSelect];
                            source->volume(source->volume() + .1f);
                        } break;

                        case SDL_SCANCODE_DOWN:
                        {
                            auto source = sources[channelSelect];
                            source->volume(source->volume() - .1f);
                        } break;

                        default:
                            break;
                    }
                } break;

                default:
                    break;
            }

            engine.update();
        }
    }
#ifdef __EMSCRIPTEN__
    }; emscripten_set_main_loop(emMainLoop, -1, 1);
#endif

    engine.close();

    SDL_Quit();
    return 0;
}
