#include <insound/AudioSpec.h>
#include <insound/Bus.h>
#include <insound/Engine.h>
#include <insound/PCMSource.h>
#include <insound/SoundBuffer.h>
#include <insound/SourceRef.h>

#include <insound/effects/DelayEffect.h>
#include <insound/effects/PanEffect.h>

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

    auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        printf("SDL renderer failed to create: %s\n", SDL_GetError());
        return -1;
    }
    SDL_SetRenderDrawColor(renderer, 128, 128, 255, 255);

    Engine engine;
    if (!engine.open())
    {
        printf("Failed to init sound engine\n");
        return -1;
    }

    AudioSpec spec;
    engine.getSpec(&spec);

    BusRef myBus;
    engine.createBus(false, &myBus);

    SoundBuffer sounds[4];
    sounds[0].load("assets/bassdrum.wav", spec);
    sounds[1].load("assets/ep.wav", spec);
    sounds[2].load("assets/piano.wav", spec);
    sounds[3].load("assets/snare-hat.wav", spec);

    SoundBuffer marimba;
    marimba.load("assets/marimba.wav", spec);

    PCMSourceRef sources[4];
    engine.playSound(&sounds[0], false, true, false, myBus, &sources[0]);
    engine.playSound(&sounds[1], false, true, false, myBus, &sources[1]);
    engine.playSound(&sounds[2], false, true, false, myBus, &sources[2]);
    engine.playSound(&sounds[3], false, true, false, myBus, &sources[3]);

    sources[0]->setVolume(.5f);

    BusRef masterBus;
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

                        case SDL_SCANCODE_F: // fade out / in pause
                        {
                            uint32_t clock;
                            if (!myBus->getParentClock(&clock))
                                break;

                            if (paused)
                            {
                                myBus->setPaused(false, clock);
                                myBus->setPaused(true, 0); // cancel pause TODO: this is an unclear way to clear timed pause
                                myBus->addFadePoint(clock, 0);
                                myBus->addFadePoint(clock + spec.freq, 1.f);
                            }
                            else
                            {
                                myBus->setPaused(true, clock + spec.freq);
                                myBus->setPaused(false, 0);
                                myBus->addFadePoint(clock, 1.f);
                                myBus->addFadePoint(clock + spec.freq, 0);
                            }

                            paused = !paused;
                        } break;

                        case SDL_SCANCODE_P: // immediate pause
                        {
                            myBus->setPaused(!paused);
                            paused = !paused;
                        } break;

                        case SDL_SCANCODE_O: { // play one shot

                            PCMSourceRef marimbaSource;
                            engine.playSound(&marimba, true, false, true, &marimbaSource);

                            if (marimbaSource.isValid())
                            {
                                marimbaSource->addEffect<DelayEffect>(0, spec.freq * .25f, .20f, .4f);
                                marimbaSource->setSpeed(2.f);
                                marimbaSource->setPaused(false);
                            }
                        } break;

                        case SDL_SCANCODE_Z: // Set myBus left pan =>
                        {
                            myBus->panner()->left(myBus->panner()->left() + .05f);
                        } break;

                        case SDL_SCANCODE_X: // Set myBus left pan <=
                        {
                            myBus->panner()->left(myBus->panner()->left() - .05f);
                        } break;

                        case SDL_SCANCODE_C: // Set myBus right pan <=
                        {
                            myBus->panner()->right(myBus->panner()->right() - .05f);
                        } break;
                        case SDL_SCANCODE_V: // Set myBus right pan =>
                        {
                            myBus->panner()->right(myBus->panner()->right() + .05f);
                        } break;

                        case SDL_SCANCODE_S: // Stop and release the sound source
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
                                source->setPosition(0);
                            }
                        } break;

                        case SDL_SCANCODE_UP:
                        {
                            auto source = sources[channelSelect];
                            source->setVolume(source->getVolume(nullptr) + .1f);
                        } break;

                        case SDL_SCANCODE_DOWN:
                        {
                            auto source = sources[channelSelect];
                            source->setVolume(source->getVolume(nullptr) - .1f);
                        } break;

                        default:
                            break;
                    }
                } break;

                default:
                    break;
            }
        }

        engine.update();

        SDL_SetRenderDrawColor(renderer, 128, 128, 255, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
#ifdef __EMSCRIPTEN__
    }; emscripten_set_main_loop(emMainLoop, -1, 1);
#endif

    engine.close();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
