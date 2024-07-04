#include "Sdl3AudioGuard.h"
#ifdef INSOUND_BACKEND_SDL3
#include "../../Error.h"

#include <SDL3/SDL.h>

#include <mutex>

int insound::detail::Sdl3AudioGuard::s_initCount;

static std::mutex s_sdlInitMutex;

insound::detail::Sdl3AudioGuard::Sdl3AudioGuard() : m_didInit(false)
{
    std::lock_guard lockGuard(s_sdlInitMutex);
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        INSOUND_PUSH_ERROR(Result::RuntimeErr, SDL_GetError());
    }
    else
    {
        m_didInit = true;
        ++s_initCount;
    }
}

insound::detail::Sdl3AudioGuard::~Sdl3AudioGuard()
{
    std::lock_guard lockGuard(s_sdlInitMutex);
    if (m_didInit)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);

        --s_initCount;

        if (s_initCount <= 0)
        {
            SDL_Quit();
        }
    }
}
#endif
