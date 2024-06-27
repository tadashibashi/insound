#include "common.h"

#include <insound/core.h>
using namespace insound;

#include <SDL2/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <functional>

std::function<void()> emMainLoopCallback;
void emMainLoop()
{
    emMainLoopCallback();
}

#endif

const std::string &getRootDir()
{
    static std::string rootDir;
    if (rootDir.empty())
    {
#ifdef __ANDROID__
        return "";
#else
        auto path = SDL_GetBasePath();
        if (path)
        {
            rootDir.assign(path);
            SDL_free(path);
        }
#endif
    }

    return rootDir;
}

int mainNoEngine()
{
    if (SDL_Init(SDL_INIT_EVENTS) != 0)
    {
        printf("Failed to init SDL2: %s\n", SDL_GetError());
        return -1;
    }

    const auto window = SDL_CreateWindow("Mixer test", SDL_WINDOWPOS_UNDEFINED,
                                         SDL_WINDOWPOS_UNDEFINED,
                                         400, 400, 0);
    if (!window)
    {
        printf("SDL window failed to create: %s\n", SDL_GetError());
        return -1;
    }

    const auto renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        printf("SDL renderer failed to create: %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetRenderDrawColor(renderer, 128, 128, 255, 255);

    bool isRunning = true;

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

                default:
                    break;
            }
        }
        SDL_SetRenderDrawColor(renderer, 128, 128, 255, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
#ifdef __EMSCRIPTEN__
    };
    emscripten_set_main_loop(emMainLoop, -1, 1);
#endif

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

int mainWithEngine()
{
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO) != 0)
    {
        printf("Failed to init SDL2 %s\n", SDL_GetError());
        return -1;
    }

     const auto window = SDL_CreateWindow("Mixer test", SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          400, 400, 0);
     if (!window)
     {
         printf("SDL window failed to create: %s\n", SDL_GetError());
         return -1;
     }

     const auto renderer = SDL_CreateRenderer(window, -1,
         SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
     if (!renderer)
     {
         printf("SDL renderer failed to create: %s\n", SDL_GetError());
         return -1;
     }
    SDL_SetRenderDrawColor(renderer, 128, 128, 255, 255);

    Engine engine;

    int bufferFrameSize = 1024;
    if (!engine.open(0, bufferFrameSize))
    {
        printf("Failed to init sound engine\n");
        return -1;
    }

    AudioSpec spec;
    engine.getSpec(&spec);

    Handle<Bus> myBus;
    engine.createBus(false, &myBus);

    AudioLoader buffers(&engine);
    buffers.setBaseDir(getRootDir());

    const SoundBuffer *sounds[4] = {
        buffers.loadAsync("assets/bassdrum.wav"),
        buffers.loadAsync("assets/ep.wav"),
        buffers.loadAsync("assets/piano.wav"),
        buffers.loadAsync("assets/snare-hat.wav"),
    };

     const SoundBuffer *pizz = buffers.loadAsync("assets/vln_pizz.ogg");
     const SoundBuffer *orch = buffers.loadAsync("assets/orch_scene.mp3");
     const SoundBuffer *arp = buffers.loadAsync("assets/arp.flac");
     const SoundBuffer *marimba = buffers.loadAsync("assets/marimba.wav");
     const SoundBuffer *bach = buffers.loadAsync("assets/test.nsf");

    Handle<PCMSource> sources[4];
    bool sourcesWereLoaded = false;

    float masterBusFade = 1.f;

    Handle<Bus> masterBus;
    engine.getMasterBus(&masterBus);

    //masterBus->addEffect<DelayEffect>(0, spec.freq * .75f, .5, .5);

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
                            engine.close();
                            SDL_DestroyRenderer(renderer);
                            SDL_DestroyWindow(window);
                            SDL_Quit();
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

                        case SDL_SCANCODE_F: // faded pause
                        {
                            uint32_t clock;
                            if (!masterBus->getParentClock(&clock))
                                break;

                            if (paused)
                            {
                                masterBus->unpauseAt(clock);
                                masterBus->pauseAt(0); // cancel pause TODO: this is an unclear way to clear timed pause
                                masterBus->fadeTo(1.f, spec.freq * .1);
                            }
                            else
                            {
                                masterBus->pauseAt(clock + spec.freq * .1);
                                masterBus->unpauseAt(0);
                                masterBus->fadeTo(0.f, spec.freq * .1);
                            }

                            paused = !paused;
                        } break;

                        case SDL_SCANCODE_P: // immediate pause
                        {
                            myBus->setPaused(!paused);

                            paused = !paused;

                            if (paused)
                            {
                                myBus->fadeTo(0, 0);
                            }
                            else
                            {
                                myBus->fadeTo(1.f, 0);
                            }
                        } break;

                        case SDL_SCANCODE_O: { // play one shot

                            Handle<PCMSource> marimbaSource;
                            engine.playSound(marimba, true, false, true, &marimbaSource);

                            if (marimbaSource.isValid())
                            {
                                marimbaSource->addEffect<DelayEffect>(0, spec.freq * .25f, .20f, .4f);
                                marimbaSource->setSpeed(2.f);
                                marimbaSource->setPaused(false);
                            }
                        } break;

                        case SDL_SCANCODE_I:
                        {
                            engine.playSound(pizz, false, false, true, nullptr);
                        } break;

                        case SDL_SCANCODE_J:
                        {
                            engine.playSound(arp, false, false, true, nullptr);
                        } break;

                        case SDL_SCANCODE_K:
                        {
                            engine.playSound(orch, false, false, true, nullptr);
                        } break;

                        case SDL_SCANCODE_B:
                        {
                            engine.playSound(bach, false, false, true, nullptr);
                        } break;

                        case SDL_SCANCODE_Z: // Set myBus left pan =>
                        {
                            Handle<PanEffect> panner;
                            myBus->getPanner(&panner);
                            panner->left(panner->left() + .05f);
                        } break;

                        case SDL_SCANCODE_X: // Set myBus left pan <=
                        {
                            Handle<PanEffect> panner;
                            myBus->getPanner(&panner);
                            panner->left(panner->left() - .05f);
                        } break;

                        case SDL_SCANCODE_C: // Set myBus right pan <=
                        {
                            Handle<PanEffect> panner;
                            myBus->getPanner(&panner);
                            panner->right(panner->right() - .05f);
                        } break;
                        case SDL_SCANCODE_V: // Set myBus right pan =>
                        {
                            Handle<PanEffect> panner;
                            myBus->getPanner(&panner);
                            panner->right(panner->right() + .05f);
                        } break;

                        case SDL_SCANCODE_S: // Stop and release the sound source
                        {
                            auto &source = sources[channelSelect];
                            if (source.isValid())
                            {
                                uint32_t parentClock;
                                source->getParentClock(&parentClock);
                                source->fadeTo(0, spec.freq * .5);
                                source->pauseAt(parentClock + spec.freq * .5, true);
                            }
                        } break;

                        case SDL_SCANCODE_N:
                        {
                            engine.playStream("assets/test.nsf", false, true, true, {}, nullptr);
                        } break;

                        // Reset the sound sources
                        case SDL_SCANCODE_R:
                        {
                            for (auto &source : sources)
                            {
                                source->setPosition(0);
                            }
                        } break;

                        case SDL_SCANCODE_UP:
                        {
                            auto source = sources[channelSelect];

                            float curVolume;
                            source->getVolume(&curVolume);
                            source->setVolume(curVolume + .1f);
                        } break;

                        case SDL_SCANCODE_DOWN:
                        {
                            auto source = sources[channelSelect];

                            float curVolume;
                            source->getVolume(&curVolume);
                            source->setVolume(curVolume - .1f);
                        } break;

                        case SDL_SCANCODE_MINUS:
                        {
                            masterBusFade -= .1f;
                            masterBus->fadeTo(masterBusFade, spec.freq * .5);
                        } break;

                        case SDL_SCANCODE_EQUALS:
                        {
                            masterBusFade += .1f;
                            masterBus->fadeTo(masterBusFade, spec.freq * .5);
                        } break;

                        case SDL_SCANCODE_M:
                        {
                            bool suspended;
                            engine.getPaused(&suspended);
                            engine.setPaused(!suspended);
                        } break;

                        default:
                            break;
                    }
                } break;

                default:
                    break;
            }
        }

        if (!sourcesWereLoaded)
        {
            bool allLoaded = true;
            for (auto sound : sounds)
            {
                if (!sound->isLoaded())
                {
                    allLoaded = false;
                    break;
                }
            }

            if (allLoaded)
            {
                sourcesWereLoaded = true;
                engine.playSound(sounds[0], true, true, false, myBus, &sources[0]);
                engine.playSound(sounds[1], true, true, false, myBus, &sources[1]);
                engine.playSound(sounds[2], true, true, false, myBus, &sources[2]);
                engine.playSound(sounds[3], true, true, false, myBus, &sources[3]);
                sources[0]->setSpeed(1.1234f);
                sources[1]->setSpeed(1.1234f);
                sources[2]->setSpeed(1.1234f);
                sources[3]->setSpeed(1.1234f);
                sources[0]->setPaused(false);
                sources[1]->setPaused(false);
                sources[2]->setPaused(false);
                sources[3]->setPaused(false);
                SDL_Log("Sounds loaded!\n");
            }
            else
            {
                SDL_Log("Not loaded yet!\n");
            }
        }

        engine.update();

        SDL_SetRenderDrawColor(renderer, 128, 128, 255, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        std::this_thread::sleep_for(std::chrono::nanoseconds(16666667));
    }
#ifdef __EMSCRIPTEN__
    };
    emscripten_set_main_loop(emMainLoop, -1, 1);
#endif

    engine.close();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
