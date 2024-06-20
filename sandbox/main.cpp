
#include <insound/core.h>

#include <SDL2/SDL.h>

struct AppContext {
    insound::Engine *audio;
    SDL_Window *window;
    SDL_Renderer *renderer;
};

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <functional>

std::function<void()> emMainLoopCallback;
void emMainLoop()
{
    emMainLoopCallback();
}
#endif

using namespace insound;

static int mainNoEngine()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("Failed to init SDL2 %s\n", SDL_GetError());
        return -1;
    }

    AppContext app{};

    const auto window = SDL_CreateWindow("Mixer test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        400, 400, 0);
    if (!window)
    {
        printf("SDL window failed to create: %s\n", SDL_GetError());
        return -1;
    }

    const auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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

static int mainWithEngine()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("Failed to init SDL2 %s\n", SDL_GetError());
        return -1;
    }

    AppContext app{};

    const auto window = SDL_CreateWindow("Mixer test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        400, 400, 0);
    if (!window)
    {
        printf("SDL window failed to create: %s\n", SDL_GetError());
        return -1;
    }

    const auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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

    app.audio = &engine;
    app.window = window;
    app.renderer = renderer;

    AudioSpec spec;
    engine.getSpec(&spec);

    Handle<Bus> myBus;
    engine.createBus(false, &myBus);

    AudioLoader buffers(&engine);

    const SoundBuffer *sounds[4] = {
        buffers.loadAsync("assets/bassdrum.wav"),
        buffers.loadAsync("assets/ep.wav"),
        buffers.loadAsync("assets/piano.wav"),
        buffers.loadAsync("assets/snare-hat.wav"),
    };

     const SoundBuffer pizz("assets/vln_pizz.ogg", spec);
     const SoundBuffer orch("assets/orch_scene.mp3", spec);
     const SoundBuffer arp("assets/arp.flac", spec);
     const SoundBuffer marimba("assets/marimba.wav", spec);
     const SoundBuffer bach("assets/test.nsf", spec);

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
                            engine.playSound(&marimba, true, false, true, &marimbaSource);

                            if (marimbaSource.isValid())
                            {
                                marimbaSource->addEffect<DelayEffect>(0, spec.freq * .25f, .20f, .4f);
                                marimbaSource->setSpeed(2.f);
                                marimbaSource->setPaused(false);
                            }
                        } break;

                        case SDL_SCANCODE_I:
                        {
                            engine.playSound(&pizz, false, false, true, nullptr);
                        } break;

                        case SDL_SCANCODE_J:
                        {
                            engine.playSound(&arp, false, false, true, nullptr);
                        } break;

                        case SDL_SCANCODE_K:
                        {
                            engine.playSound(&orch, false, false, true, nullptr);
                        } break;

                        case SDL_SCANCODE_B:
                        {
                            engine.playSound(&bach, false, false, true, nullptr);
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
                printf("Sounds loaded!\n");
            }
            else
            {
                printf("Not loaded yet!\n");
            }
        }

        engine.update();

        SDL_SetRenderDrawColor(renderer, 128, 128, 255, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
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

int main(int argc, char *argv[])
{
    return mainWithEngine();
}
