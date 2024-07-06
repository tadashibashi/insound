cmake_minimum_required(VERSION 3.20 FATAL_ERROR)
project(insound)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW CACHE STRING "")
cmake_policy(SET CMP0077 NEW)

# C++ Standard
set(CMAKE_CXX_STANDARD_REQUIRED 17)
if (NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()

# No c++20 modules in this project
if (CMAKE_CXX_STANDARD GREATER_EQUAL 20)
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

# Add ./cmake to CMake modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Check if this is the root project
set(INSOUND_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE STRING "" FORCE)
if (CMAKE_SOURCE_DIR STREQUAL INSOUND_ROOT_DIR)
    set(INSOUND_IS_ROOT TRUE)
else()
    set(INSOUND_IS_ROOT FALSE)
endif()

if (TARGET SDL2::SDL2)
    set(INSOUND_USE_CLIENT_SDL2_DEFAULT ON)
else()
    set(INSOUND_USE_CLIENT_SDL2_DEFAULT OFF)
endif()

if (TARGET SDL3::SDL3)
    set(INSOUND_USE_CLIENT_SDL3_DEFAULT ON)
else()
    set(INSOUND_USE_CLIENT_SDL3_DEFAULT OFF)
endif()

# Insound Audio Engine options

# SDL (this is currently not working, as all backends do not depend on SDL at the moment)
option(INSOUND_BACKEND_SDL2      "Enable SDL2 backend"                                  OFF )
option(INSOUND_USE_CLIENT_SDL2   "Use user's SDL2::SDL2 target"                       ${INSOUND_USE_CLIENT_SDL2_DEFAULT})
option(INSOUND_BACKEND_SDL3      "Enable SDL3 backend"                                  OFF )
option(INSOUND_USE_CLIENT_SDL3   "Use user's SDL3::SDL3 target"                       ${INSOUND_USE_CLIENT_SDL3_DEFAULT})

# Audio file decoders
option(INSOUND_DECODE_MP3        "Include mp3 file decoding in build."                  ON )
option(INSOUND_DECODE_FLAC       "Include flac file decoding in build."                 ON )
option(INSOUND_DECODE_WAV        "Include wav file decoding in build."                  ON )
option(INSOUND_DECODE_VORBIS     "Include ogg vorbis file decoding in build."           ON )
option(INSOUND_DECODE_GME        "Include classic game emu decoding in build."          ON )
option(INSOUND_DECODE_MODPLUG    "Include modplug format decoding in build."            ON )


option(INSOUND_LOGGING           "Enable logging in release mode"                       ON )

option(INSOUND_NO_PTHREAD        "Turn pthreads off. (Only affects Emscripten builds)"  OFF)

option(INSOUND_BUILD_TESTS       "Include insound test targets."                        ${INSOUND_IS_ROOT})
option(INSOUND_BUILD_EXAMPLES    "Include insound example project targets."             ${INSOUND_IS_ROOT})

option(INSOUND_CPU_INTRINSICS    "Turn on CPU intrinsics"                               ON )

if (NOT EMSCRIPTEN)
    set (INSOUND_NO_PTHREAD OFF CACHE BOOL "Non-emscripten platforms must have pthreads on" FORCE)
endif()

add_subdirectory(insound)
if (INSOUND_BUILD_TESTS)
    add_subdirectory(tests)
endif()
