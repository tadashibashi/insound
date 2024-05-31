project(insound)

if (NOT TARGET SDL2::SDL2-static)
    include(FetchContent)
    FetchContent_Declare(SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        fb1497566c5a05e2babdcf45ef0ab5c7cca2c4ae
    )
    FetchContent_MakeAvailable(SDL2)

    set(SDL_STATIC      ON  CACHE BOOL "" FORCE)
    set(SDL_SHARED      OFF CACHE BOOL "" FORCE)
    set(SDL_TEST        OFF CACHE BOOL "" FORCE)
endif()

add_library(insound
    Engine.cpp
    Engine.h
    SoundBuffer.cpp
    SoundBuffer.h
    PCMSource.cpp
    PCMSource.h
    Pool.h
    Bus.cpp
    Bus.h
    SoundSource.h
    error.cpp
    error.h
    Effect.cpp
    Effect.h
    effects/PanEffect.cpp
    effects/PanEffect.h
    SoundSource.cpp
    mathf.cpp
    mathf.h
    Command.h
    effects/VolumeEffect.cpp
    effects/VolumeEffect.h
    SampleFormat.cpp
    SampleFormat.h
    AudioSpec.cpp
    AudioSpec.h
    private/SdlAudioGuard.cpp
    private/SdlAudioGuard.h
    SourceHandle.h
    effects/DelayEffect.cpp
    effects/DelayEffect.h
)

target_link_libraries(insound PRIVATE SDL2::SDL2-static)
target_include_directories(insound PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
if ("${CMAKE_BUILD_TYPE}" MATCHES "Deb")
    target_compile_definitions(insound PRIVATE -DINSOUND_DEBUG=1)
else()
    target_compile_definitions(insound PRIVATE -DINSOUND_DEBUG=0)
endif()

if (EMSCRIPTEN)
    target_link_options(insound PUBLIC
        -sALLOW_MEMORY_GROWTH=1
    )
endif()