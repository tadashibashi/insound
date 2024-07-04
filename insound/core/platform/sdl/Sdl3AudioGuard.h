#pragma once
#ifdef INSOUND_BACKEND_SDL3
namespace insound::detail {
    /// Thread-safe guard that initializes and quits SDL to provide its audio functionality during its lifetime.
    /// It may be instantiated multiple times, as it will count references before fully quitting SDL.
    class Sdl3AudioGuard {
    public:
        Sdl3AudioGuard();
        ~Sdl3AudioGuard();

    private:
        bool m_didInit;         ///< whether this class succeeded in initializing sdl audio
        static int s_initCount; ///< total times sdl audio init was successfully called, when it hits
                                ///< 0 during this class's destructor, it will call a full `SDL_Quit`
    };
}
#endif
