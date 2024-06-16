#include <iostream>
#include <insound/AudioLoader.h>
#include <insound/AudioSpec.h>
#include <insound/Bus.h>
#include <insound/Engine.h>
#include <insound/PCMSource.h>
#include <insound/SoundBuffer.h>
#include <insound/SourceRef.h>

#include <insound/effects/DelayEffect.h>
#include <insound/effects/PanEffect.h>
#include <insound/io/openFile.h>

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
static const char *unloadCallback(int eventType, const void *reserved, void *userData)
{
    printf("Window closed\n");

    auto context = (AppContext *)userData;
    context->audio->close();

    SDL_DestroyRenderer(context->renderer);
    SDL_DestroyWindow(context->window);
    SDL_Quit();
    emscripten_force_exit(0);
    return "";
}
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

    auto t = std::thread([]() {
        std::string testData;
        if (openFile("https://httpbin.org/get", &testData))
        {
            std::cout << "Retrieved data: " << testData << '\n';
        }
        else
        {
            std::cout << "Failed to retrieve data: " << popError().message << '\n';
        }
    });




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

    int bufferFrameSize = 512;
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

     // const SoundBuffer pizz("assets/vln_pizz.ogg", spec);
     // const SoundBuffer orch("assets/orch_scene.mp3", spec);
     // const SoundBuffer arp("assets/arp.flac", spec);
     // const SoundBuffer marimba("assets/marimba.wav", spec);
     // const SoundBuffer bach("assets/test.nsf", spec);

    Handle<PCMSource> sources[4];
    bool sourcesWereLoaded = false;

    //sources[0]->setVolume(.5f);

    float masterBusFade = 1.f;

    Handle<Bus> masterBus;
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
                            if (!myBus->getParentClock(&clock))
                                break;

                            if (paused)
                            {
                                myBus->setPaused(false, clock);
                                myBus->setPaused(true, 0); // cancel pause TODO: this is an unclear way to clear timed pause
                                myBus->fadeTo(1.f, spec.freq * .1);
                            }
                            else
                            {
                                myBus->setPaused(true, clock + spec.freq * .1);
                                myBus->setPaused(false, 0);
                                myBus->fadeTo(0.f, spec.freq * .1);
                            }

                            paused = !paused;
                        } break;

                        case SDL_SCANCODE_P: // immediate pause
                        {
                            myBus->setPaused(!paused);
                            paused = !paused;
                        } break;

                        // case SDL_SCANCODE_O: { // play one shot
                        //
                        //     Handle<PCMSource> marimbaSource;
                        //     engine.playSound(&marimba, true, false, true, &marimbaSource);
                        //
                        //     if (marimbaSource.isValid())
                        //     {
                        //         marimbaSource->addEffect<DelayEffect>(0, spec.freq * .25f, .20f, .4f);
                        //         marimbaSource->setSpeed(2.f);
                        //         marimbaSource->setPaused(false);
                        //     }
                        // } break;
                        //
                        // case SDL_SCANCODE_I:
                        // {
                        //     engine.playSound(&pizz, false, false, true, nullptr);
                        // } break;
                        //
                        // case SDL_SCANCODE_J:
                        // {
                        //     engine.playSound(&arp, false, false, true, nullptr);
                        // } break;
                        //
                        // case SDL_SCANCODE_K:
                        // {
                        //     engine.playSound(&orch, false, false, true, nullptr);
                        // } break;
                        //
                        // case SDL_SCANCODE_B:
                        // {
                        //     engine.playSound(&bach, false, false, true, nullptr);
                        // } break;

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
                            if (sources[channelSelect].isValid())
                            {
                                engine.releaseSound(sources[channelSelect]);
                            }
                        } break;

                        // Reset the sound sources
                        case SDL_SCANCODE_R:
                        {
                            for (auto &source : sources)
                            {
                                source->setPosition(0);
                            }
                            printf("Restarted track\n");
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
                engine.playSound(sounds[0], false, true, false, myBus, &sources[0]);
                engine.playSound(sounds[1], false, true, false, myBus, &sources[1]);
                engine.playSound(sounds[2], false, true, false, myBus, &sources[2]);
                engine.playSound(sounds[3], false, true, false, myBus, &sources[3]);
                std::cout << "Sounds loaded!\n";
            }
            else
            {
                std::cout << "Not loaded yet!\n";
            }
        }

        engine.update();

        SDL_SetRenderDrawColor(renderer, 128, 128, 255, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
#ifdef __EMSCRIPTEN__
    };
    emscripten_set_beforeunload_callback(&app, unloadCallback);
    emscripten_set_main_loop(emMainLoop, -1, 1);
#endif

    engine.close();
    t.join();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
