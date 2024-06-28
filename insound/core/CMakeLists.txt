project(insound)
include(FetchContent)

add_library(insound)

if (NOT TARGET SDL2::SDL2-static) # user can provide their own SDL2 submodule, but we may want to enforce a minimum version
    FetchContent_Declare(Insound_SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        fb1497566c5a05e2babdcf45ef0ab5c7cca2c4ae
    )

    set(SDL_STATIC             ON )
    set(SDL_SHARED             OFF)
    set(SDL2_DISABLE_UNINSTALL ON )
    set(SDL2_DISABLE_INSTALL   ON )
    set(SDL_TEST               OFF)
    set(SDL_ASSEMBLY           ON)
    set(SDL_ARMNEON            ON)
    set(SDL_PTHREADS           NOT ${INSOUND_NO_PTHREAD})

    FetchContent_MakeAvailable(Insound_SDL2)
endif()

if (INSOUND_DECODE_VORBIS)
    FetchContent_Declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        013ac3beddff3dbffafd5177e7972067cd2b5083
    )
    FetchContent_MakeAvailable(stb)
    FetchContent_GetProperties(stb)

    target_include_directories(insound PRIVATE ${stb_SOURCE_DIR})
    target_compile_definitions(insound PRIVATE -DINSOUND_DECODE_VORBIS=1)
endif()

if (INSOUND_DECODE_FLAC OR INSOUND_DECODE_MP3)
    FetchContent_Declare(dr_libs
        GIT_REPOSITORY https://github.com/mackron/dr_libs.git
        GIT_TAG        da35f9d6c7374a95353fd1df1d394d44ab66cf01
    )
    FetchContent_MakeAvailable(dr_libs)
    FetchContent_GetProperties(dr_libs)

    target_include_directories(insound PRIVATE ${dr_libs_SOURCE_DIR})

    if (INSOUND_DECODE_FLAC)
        target_compile_definitions(insound PRIVATE -DINSOUND_DECODE_FLAC=1)
    endif()

    if (INSOUND_DECODE_MP3)
        target_compile_definitions(insound PRIVATE -DINSOUND_DECODE_MP3=1)
    endif()
endif()

if (INSOUND_DECODE_GME)
    FetchContent_Declare(libgme
        GIT_REPOSITORY https://github.com/libgme/game-music-emu.git
        GIT_TAG        4964e543bc25ac3d7968fb5117aabd99d7ff2073
    )

    if (EMSCRIPTEN)
        set(GME_BUILD_SHARED  OFF)
    else()
        set(GME_BUILD_SHARED  ON)  # leave this ON to avoid forcing consumer app to be LGPL
    endif()
    set(GME_ENABLE_UBSAN      OFF)

    set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED TRUE)

    FetchContent_MakeAvailable(libgme)
    target_link_libraries(insound PUBLIC gme::gme)
    target_compile_definitions(insound PRIVATE -DINSOUND_DECODE_GME=1)
endif()

if (INSOUND_CPU_INTRINSICS)
    target_compile_definitions(insound PUBLIC -DINSOUND_CPU_INTRINSICS)
endif()

target_sources(insound
PUBLIC
    AlignedVector.h
    AudioDecoder.h
    AudioDevice.h
    AudioLoader.h
    AudioSpec.h
    Bus.h
    BufferView.h
    Command.h
    CpuIntrinsics.h
    Effect.h
    effects/DelayEffect.h
    effects/PanEffect.h
    effects/VolumeEffect.h
    Engine.h
    Error.h
    Handle.h
    io/loadAudio.h
    io/decodeWAV.h
    io/decodeVorbis.h
    io/decodeMp3.h
    io/decodeFLAC.h
    io/decodeGME.h
    io/Rstream.h
    logging.h
    Marker.h
    MultiPool.h
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
    AudioDecoder.cpp
    AudioDevice.cpp
    AudioLoader.cpp
    Bus.cpp
    BufferView.cpp
    Effect.cpp
    effects/DelayEffect.cpp
    effects/PanEffect.cpp
    effects/VolumeEffect.cpp
    external/ctpl_stl.h
    Engine.cpp
    Error.cpp
    io/decodeFLAC.cpp
    io/decodeGME.cpp
    io/decodeMp3.cpp
    io/decodeVorbis.cpp
    io/decodeWAV.cpp
    io/openFile.cpp
    io/openFile.h
    io/loadAudio.cpp
    io/Rstreamable.h
    io/RStream.cpp
    io/RstreamableAAsset.h
    io/RstreamableAAsset.cpp
    io/RstreamableFile.cpp
    io/RstreamableFile.h
    PCMSource.cpp
    PerfTimer.cpp
    platform/AndroidNative.cpp
    platform/AndroidNative.h
    platform/AudioDevice/AAudioDevice.cpp
    platform/AudioDevice/AAudioDevice.h
    platform/AudioDevice/SdlAudioDevice.cpp
    platform/AudioDevice/SdlAudioDevice.h
    platform/AudioDevice/EmAudioDevice.cpp
    platform/AudioDevice/EmAudioDevice.h
    platform/getDefaultSampleRate.cpp
    platform/getDefaultSampleRate.h
    Pool.cpp
    SampleFormat.cpp
    SoundBuffer.cpp
    Source.cpp
    StreamSource.cpp
    StreamManager.cpp
    TimeUnit.cpp
    util.cpp

    private/SdlAudioGuard.cpp
    private/SdlAudioGuard.h
)

target_link_libraries(insound PRIVATE SDL2::SDL2-static)
target_include_directories(insound PUBLIC ${INSOUND_ROOT_DIR})

if ("${CMAKE_BUILD_TYPE}" MATCHES "Deb")
    set(INSOUND_DEBUG TRUE)
    target_compile_definitions(insound PUBLIC -DINSOUND_DEBUG=1)
else()
    set(INSOUND_DEBUG FALSE)
endif()

if (INSOUND_LOGGING)
    target_compile_definitions(insound PUBLIC -DINSOUND_LOGGING=1)
endif()

if (ANDROID)
    target_link_libraries(insound PRIVATE aaudio)
endif()

if (EMSCRIPTEN)
    target_link_options(insound
    PUBLIC
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=2GB
        -sSTACK_SIZE=131072
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

    if (INSOUND_DEBUG)
        target_link_options(insound PRIVATE -O0 -g -g3 -gsource-map)
        target_compile_options(insound PRIVATE -O0 -g -g3 -gsource-map)
    else()
        target_link_options(insound PRIVATE -Ofast)
        target_compile_options(insound PRIVATE -Ofast)
    endif()
else() # Non-emscripten platforms
    if (INSOUND_DEBUG)
        target_link_options(insound PRIVATE -O0 -g)
        target_compile_options(insound PRIVATE -O0 -g)
    else()
        target_link_options(insound PRIVATE -Ofast -flto)
        target_compile_options(insound PRIVATE -Ofast -flto)
    endif()
endif()

if (NOT INSOUND_NO_PTHREAD)
    target_compile_definitions(insound PRIVATE -DINSOUND_THREADING)
endif()
