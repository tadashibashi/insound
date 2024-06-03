project(insound)

if (NOT TARGET SDL2::SDL2-static)
    include(FetchContent)
    FetchContent_Declare(SDL2
        GIT_REPOSITORY https://github.com/tadashibashi/SDL.git
        GIT_TAG        78b6ce7e0c524422ea5deb4041fada5b60ca2585
    )
    FetchContent_MakeAvailable(SDL2)

    set(SDL_STATIC      ON  CACHE BOOL "" FORCE)
    set(SDL_SHARED      OFF CACHE BOOL "" FORCE)
    set(SDL_TEST        OFF CACHE BOOL "" FORCE)
endif()

add_library(insound
    AudioSpec.h
    Bus.cpp
    Bus.h
    Command.h
    Effect.cpp
    Effect.h
    effects/DelayEffect.cpp
    effects/DelayEffect.h
    effects/PanEffect.cpp
    effects/PanEffect.h
    effects/VolumeEffect.cpp
    effects/VolumeEffect.h
    Engine.cpp
    Engine.h
    error.cpp
    error.h
    mathf.cpp
    mathf.h
    PCMSource.cpp
    PCMSource.h
    Pool.h
    private/SdlAudioGuard.cpp
    private/SdlAudioGuard.h
    SampleFormat.cpp
    SampleFormat.h
    SoundBuffer.cpp
    SoundBuffer.h
    SoundSource.cpp
    SoundSource.h
    SourceRef.h
    SourceRefFwd.h
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
else()
    target_compile_definitions(insound PRIVATE -DINSOUND_THREADING=1)
endif()
