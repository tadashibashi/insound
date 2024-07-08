if (APPLE AND NOT EMSCRIPTEN AND NOT ANDROID)
    project(insound OBJC OBJCXX C CXX)
else()
    project(insound C CXX)
endif()
include(FetchContent)
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED TRUE)

add_library(insound)

# ===== Audio Backends ========================================================

if (IOS) # backend currently breaks if too many dropouts occur, default to SDL2 instead
    target_sources(insound PRIVATE
        platform/ios/iOSAudioDevice.mm
        platform/ios/iOSAudioDevice.h)
    target_link_libraries(insound PRIVATE "-framework AudioToolbox" "-framework AVFoundation")
elseif (ANDROID)
    # AAudio backend for Android.
    # TODO: Fallback to OpenSL for older devices, or use Oboe to handle this.
    target_sources(insound PRIVATE
        platform/android/AAudioDevice.cpp
        platform/android/AAudioDevice.h
        platform/android/AndroidNative.cpp
        platform/android/AndroidNative.h)
    target_link_libraries(insound PRIVATE aaudio)
elseif (EMSCRIPTEN)
    # WebAudio AudioWorklet backend for Emscripten.
    # TODO: create fallback to ScripteProcessorNode via SDL, or other solution for non-threaded builds
    target_sources(insound PRIVATE
        platform/EmAudioDevice.h platform/EmAudioDevice.cpp)
    target_link_options(insound
        PUBLIC
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=2GB
        -sSTACK_SIZE=4MB
        -sASSERTIONS=2
        -sWASM=1
        -sFETCH
    )

    if (INSOUND_CPU_INTRINSICS)
        target_link_options(insound
            PUBLIC
            -msimd128
        )
        target_compile_options(insound
            PUBLIC
            -msimd128
        )
    endif()

    if (NOT INSOUND_NO_PTHREAD)
        target_link_options(insound PUBLIC
            -pthread
            -sPTHREAD_POOL_SIZE=4
            -sAUDIO_WORKLET=1
            -sWASM_WORKERS=1
        )
        target_compile_options(insound PUBLIC -pthread)
    endif()
elseif ((UNIX OR WIN32) AND NOT IOS)
    # Use PortAudio for desktop platforms
    FetchContent_Declare(PortAudio
        GIT_REPOSITORY https://github.com/PortAudio/portaudio
        GIT_TAG        2f61007cb2b57c8fba84c8fe0672830ea733f189
    )

    FetchContent_MakeAvailable(PortAudio)
    target_link_libraries(insound PRIVATE PortAudio)
    target_compile_definitions(insound PRIVATE -DINSOUND_BACKEND_PORTAUDIO=1)
    target_sources(insound PRIVATE
        platform/PortAudioDevice.h platform/PortAudioDevice.cpp)
elseif(INSOUND_BACKEND_SDL3)
    if (INSOUND_USE_CLIENT_SDL3)
        if (NOT TARGET SDL3::SDL3)
            find_package(SDL3 REQUIRED)
        endif()
        target_link_libraries(insound PUBLIC SDL3::SDL3)
    else()
        FetchContent_Declare(SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG        17d4f8d6990d2bd92d6a8be4b23c4dc1626fb923
        )

        set(SDL_STATIC             ON  CACHE BOOL "")
        set(SDL_SHARED             OFF CACHE BOOL "")
        set(SDL3_DISABLE_UNINSTALL ON  CACHE BOOL "")
        set(SDL3_DISABLE_INSTALL   ON  CACHE BOOL "")
        set(SDL_TEST               OFF CACHE BOOL "")
        set(SDL_ASSEMBLY           ON  CACHE BOOL "")
        set(SDL_ARMNEON            ON  CACHE BOOL "")
        set(SDL_PTHREADS           NOT ${INSOUND_NO_PTHREAD} CACHE BOOL "")

        # Remove unnecessary modules for quicker build
        set(SDL_VIDEO              OFF CACHE BOOL "")
        set(SDL_RENDER             OFF CACHE BOOL "")
        set(SDL_JOYSTICK           OFF CACHE BOOL "")
        set(SDL_HAPTIC             OFF CACHE BOOL "")
        set(SDL_HIDAPI             OFF CACHE BOOL "")
        set(SDL_FILE               OFF CACHE BOOL "")
        set(SDL_CPUINFO            OFF CACHE BOOL "")
        set(SDL_FILESYSTEM         OFF CACHE BOOL "")
        set(SDL_SENSOR             OFF CACHE BOOL "")
        set(SDL_LOCALE             OFF CACHE BOOL "")
        set(SDL_MISC               OFF CACHE BOOL "")

        FetchContent_MakeAvailable(SDL3)
    endif()
    target_sources(insound PRIVATE
        platform/sdl/Sdl3AudioDevice.cpp
        platform/sdl/Sdl3AudioDevice.h
        platform/sdl/Sdl3AudioGuard.cpp
        platform/sdl/Sdl3AudioGuard.h)
    target_link_libraries(insound PRIVATE SDL3::SDL3-static)
    target_compile_definitions(insound PRIVATE INSOUND_BACKEND_SDL3=1)
else()

    # Fallback to SDL2 for other platforms
    if (INSOUND_USE_CLIENT_SDL2)
        if (NOT TARGET SDL2::SDL2)
            find_package(SDL2 REQUIRED)
        endif()
        target_link_libraries(insound PUBLIC SDL2::SDL2) # link to what the user is using
    else()
        FetchContent_Declare(SDL2
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG        fb1497566c5a05e2babdcf45ef0ab5c7cca2c4ae)

        set(SDL_STATIC             ON  CACHE BOOL "")
        set(SDL_SHARED             OFF CACHE BOOL "")
        set(SDL2_DISABLE_UNINSTALL ON  CACHE BOOL "")
        set(SDL2_DISABLE_INSTALL   ON  CACHE BOOL "")
        set(SDL_TEST               OFF CACHE BOOL "")
        set(SDL_ASSEMBLY           ON  CACHE BOOL "")
        set(SDL_ARMNEON            ON  CACHE BOOL "")
        set(SDL_PTHREADS           NOT ${INSOUND_NO_PTHREAD} CACHE BOOL "")
        set(SDL_VIRTUAL_JOYSTICK   OFF CACHE BOOL "")
        set(SDL_DUMMYVIDEO         OFF CACHE BOOL "")
        set(SDL_COCOA              OFF CACHE BOOL "")

        # Remove unnecessary modules for quicker build
        set(SDL_VIDEO              OFF CACHE BOOL "")
        set(SDL_RENDER             OFF CACHE BOOL "")
        set(SDL_JOYSTICK           OFF CACHE BOOL "")
        set(SDL_HAPTIC             OFF CACHE BOOL "")
        set(SDL_HIDAPI             OFF CACHE BOOL "")
        set(SDL_FILE               OFF CACHE BOOL "")
        set(SDL_CPUINFO            OFF CACHE BOOL "")
        set(SDL_FILESYSTEM         OFF CACHE BOOL "")
        set(SDL_SENSOR             OFF CACHE BOOL "")
        set(SDL_LOCALE             OFF CACHE BOOL "")
        set(SDL_MISC               OFF CACHE BOOL "")

        FetchContent_MakeAvailable(SDL2)
        target_link_libraries(insound PRIVATE SDL2::SDL2-static) # private static
    endif()

    target_sources(insound PRIVATE
        platform/sdl/SdlAudioDevice.cpp
        platform/sdl/SdlAudioDevice.h
        platform/sdl/SdlAudioGuard.cpp
        platform/sdl/SdlAudioGuard.h)
    target_compile_definitions(insound PRIVATE INSOUND_BACKEND_SDL2=1)

    set(INSOUND_BACKEND_SDL2 ON)
endif()

# ===== Decoders ==============================================================

if (INSOUND_DECODE_FLAC)
    target_compile_definitions(insound PRIVATE -DINSOUND_DECODE_FLAC=1)
endif()
if (INSOUND_DECODE_MP3)
    target_compile_definitions(insound PRIVATE -DINSOUND_DECODE_MP3=1)
endif()
if (INSOUND_DECODE_WAV)
    target_compile_definitions(insound PRIVATE -DINSOUND_DECODE_WAV=1)
endif()

if (INSOUND_DECODE_VORBIS)
    FetchContent_Declare(vorbis
        GIT_REPOSITORY https://github.com/xiph/vorbis.git
        GIT_TAG        "v1.3.7"
    )

    FetchContent_Declare(ogg
        GIT_REPOSITORY https://github.com/xiph/ogg.git
        GIT_TAG        "v1.3.5"
    )

    set(OGG_LIBRARY $<TARGET_FILE:ogg>)
    set(OGG_INCLUDE_DIR ${ogg_SOURCE_DIR}/include)
    set(BUILD_TESTING OFF)

    FetchContent_MakeAvailable(ogg)
    FetchContent_MakeAvailable(vorbis)
    target_compile_definitions(insound PRIVATE -DINSOUND_DECODE_VORBIS=1)
    target_link_libraries(insound PRIVATE vorbisfile)
else()
    target_compile_definitions(insound PRIVATE -DMA_NO_LIBVORBIS)
endif()

if (INSOUND_DECODE_GME)
    FetchContent_Declare(libgme
        GIT_REPOSITORY https://github.com/tadashibashi/game-music-emu.git
        GIT_TAG        23e8814d9575b8bf89dd88c9b624df8d1d364a2b
    )

    if (EMSCRIPTEN)
        set(GME_BUILD_SHARED  OFF)
    else()
        set(GME_BUILD_SHARED  ON)  # leave this ON to avoid forcing consumer app to be LGPL
    endif()
    set(GME_ENABLE_UBSAN      OFF)

    FetchContent_MakeAvailable(libgme)
    target_link_libraries(insound PUBLIC gme::gme)
    target_compile_definitions(insound PRIVATE -DINSOUND_DECODE_GME=1)
    #target_sources(insound PUBLIC io/decodeGME.h PRIVATE io/decodeGME.cpp)
endif()

if (INSOUND_DECODE_MODPLUG)

endif()


# ===== Platform-independent libraries and source =============================

target_sources(insound
PUBLIC
    AlignedVector.h
    AudioDevice.h
    AudioDecoder.h
    AudioLoader.h
    AudioSpec.h
    Bus.h
    BufferView.h
    Command.h
    CpuIntrinsics.h
    DataConverter.h
    Effect.h
    effects/DelayEffect.h
    effects/PanEffect.h
    effects/VolumeEffect.h
    Engine.h
    Error.h
    Handle.h
    io/loadAudio.h
    io/Rstream.h
    logging.h
    Marker.h
    MultiPool.h
    path.h
    PCMSource.h
    PerfTimer.h
    Pool.h
    SampleFormat.h
    SoundBuffer.h
    Source.h
    SourceRef.h
    SourceRefFwd.h
    StreamManager.h
    StreamSource.h
    TimeUnit.h
    util.h

PRIVATE
    AudioDevice.cpp
    AudioDecoder.cpp
    AudioLoader.cpp
    Bus.cpp
    BufferView.cpp
    DataConverter.cpp
    Effect.cpp
    effects/DelayEffect.cpp
    effects/PanEffect.cpp
    effects/VolumeEffect.cpp
    external/ctpl_stl.cpp
    external/miniaudio.cpp
    external/miniaudio_libvorbis.cpp
    Engine.cpp
    Error.cpp
    io/openFile.cpp
    io/openFile.h
    io/loadAudio.cpp
    io/Rstreamable.cpp
    io/Rstreamable.h
    io/RStream.cpp
    io/RstreamableAAsset.h
    io/RstreamableAAsset.cpp
    io/RstreamableFile.cpp
    io/RstreamableFile.h
    io/RstreamableMemory.h
    io/RstreamableMemory.cpp
    path.cpp
    PCMSource.cpp
    PerfTimer.cpp
    Pool.cpp
    SampleFormat.cpp
    SoundBuffer.cpp
    Source.cpp
    StreamSource.cpp
    StreamManager.cpp
    TimeUnit.cpp
    util.cpp
)

target_include_directories(insound PUBLIC ${INSOUND_ROOT_DIR})


# ===== Optimization / Debug flags ====================================================

if ("${CMAKE_BUILD_TYPE}" MATCHES "Deb")
    set(INSOUND_DEBUG TRUE)
    target_compile_definitions(insound PUBLIC -DINSOUND_DEBUG=1)
else()
    set(INSOUND_DEBUG FALSE)
endif()

if (INSOUND_CPU_INTRINSICS)
    target_compile_definitions(insound PUBLIC -DINSOUND_CPU_INTRINSICS)
endif()

if (INSOUND_LOGGING)
    target_compile_definitions(insound PUBLIC -DINSOUND_LOGGING=1)
endif()

if (NOT INSOUND_NO_PTHREAD)
    target_compile_definitions(insound PRIVATE -DINSOUND_THREADING)
endif()

if (EMSCRIPTEN)
    if (INSOUND_DEBUG)
        # Optimization must set to -O1 in browsers. For whatever reason -O0 causes a mysterious stack overflow
        # that does not manifest in any other platforms.
        target_link_options(insound PUBLIC -O1 -g -g3 -gsource-map)
        target_compile_options(insound PUBLIC -O1 -g -g3 -gsource-map)
    else()
        target_link_options(insound PUBLIC -Ofast)
        target_compile_options(insound PUBLIC -Ofast)
    endif()
else() # Non-emscripten platforms
    if (INSOUND_DEBUG)
        target_link_options(insound PRIVATE -O0 -g)
        target_compile_options(insound PRIVATE -O0 -g)
    elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        target_link_options(insound PRIVATE -Oz)
        target_compile_options(insound PRIVATE -Oz)
    else()
        target_link_options(insound PRIVATE -Ofast -flto)
        target_compile_options(insound PRIVATE -Ofast -flto)
    endif()
endif()

target_link_options(insound PRIVATE --exclude-libs)
