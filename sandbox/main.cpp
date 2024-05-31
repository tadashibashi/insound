
#include <insound/Engine.h>
#include <insound/SoundBuffer.h>
#include <insound/PCMSource.h>
#include <insound/SourceHandle.h>
#include <insound/Bus.h>

#include <SDL2/SDL.h>

#include <iostream>

#include "insound/AudioSpec.h"
#include "insound/effects/DelayEffect.h"

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
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
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

    auto myBus = engine.createBus(false);

    SoundBuffer sounds[4];
    sounds[0].load("assets/bassdrum.wav", spec);
    sounds[1].load("assets/ep.wav", spec);
    sounds[2].load("assets/piano.wav", spec);
    sounds[3].load("assets/snare-hat.wav", spec);

    SourceHandle<PCMSource> sources[4];
    sources[0] = engine.playSound(&sounds[0], false, true, false, myBus);
    sources[1] = engine.playSound(&sounds[1], false, true, false, myBus);
    sources[2] = engine.playSound(&sounds[2], false, true, false, myBus);
    sources[3] = engine.playSound(&sounds[3], false, true, false, myBus);

    sources[0]->volume(.5f);

    Bus *masterBus;
    engine.getMasterBus(&masterBus);

    masterBus->insertEffect(new DelayEffect(&engine, (int)((float)spec.freq * .15f), .2f, .6f), 0);

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
                            std::cout << "Send pause command at " << myBus->parentClock() << '\n';
                            if (paused)
                            {
                                myBus->paused(false, myBus->parentClock());
                                myBus->paused(true, 0); // cancel pause TODO: this is an unclear way to clear timed pause
                                myBus->addFadePoint(myBus->parentClock(), 0);
                                myBus->addFadePoint(myBus->parentClock() + spec.freq, 1.f);
                            }
                            else
                            {
                                myBus->paused(true, myBus->parentClock() + spec.freq);
                                myBus->paused(false, 0);
                                myBus->addFadePoint(myBus->parentClock(), 1.f);
                                myBus->addFadePoint(myBus->parentClock() + spec.freq, 0);
                            }

                            paused = !paused;
                        } break;

                        case SDL_SCANCODE_Z:
                        {
                            myBus->panner()->left(myBus->panner()->left() + .05f);
                        } break;
                        case SDL_SCANCODE_X:
                        {
                            myBus->panner()->left(myBus->panner()->left() - .05f);
                        } break;
                        case SDL_SCANCODE_C:
                        {
                            myBus->panner()->right(myBus->panner()->right() - .05f);
                        } break;
                        case SDL_SCANCODE_V:
                        {
                            myBus->panner()->right(myBus->panner()->right() + .05f);
                        } break;

                        // Stop and release the sound source
                        case SDL_SCANCODE_S:
                        {
                            if (sources[channelSelect].isValid())
                            {
                                sources[channelSelect]->release();
                            }
                        } break;

                        // Reset the sound sources
                        case SDL_SCANCODE_R:
                        {
                            for (auto &source : sources)
                            {
                                if (source.isValid())
                                {
                                    source->position(0);
                                }
                            }
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
