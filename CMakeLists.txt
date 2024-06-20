cmake_minimum_required(VERSION 3.20)
project(insound)

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


# Insound Audio Engine options
# Audio file decoder
option(INSOUND_DECODE_VORBIS  "Include ogg vorbis file decoding in build."           ON)
option(INSOUND_DECODE_MP3     "Include mp3 file decoding in build."                  ON)
option(INSOUND_DECODE_FLAC    "Include flac file decoding in build."                 ON)
option(INSOUND_DECODE_GME     "Include classic game emu decoding in build."          ON)

option(INSOUND_NO_PTHREAD     "Turn pthreads off. (Only affects Emscripten builds)"  OFF)

option(INSOUND_BUILD_TESTS    "Include insound test targets."                        ${INSOUND_IS_ROOT})
option(INSOUND_BUILD_EXAMPLES "Include insound example project targets."             ${INSOUND_IS_ROOT})

option(INSOUND_CPU_INTRINSICS "Turn on CPU intrinsics"                               ON)

if (NOT EMSCRIPTEN)
    set (INSOUND_NO_PTHREAD OFF CACHE BOOL "Non-emscripten platforms must have pthreads on" FORCE)
endif()

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
endif()

add_subdirectory(insound)

if (INSOUND_BUILD_EXAMPLES)
    add_subdirectory(sandbox)
endif()

if (INSOUND_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Support for Sublime LSP-clangd and VSCode clangd
if (CMAKE_EXPORT_COMPILE_COMMANDS AND EXISTS ${CMAKE_SOURCE_DIR}/build)
    cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL _end_of_configure())
    function(_end_of_configure)
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR}/build/compile_commands.json)
    endfunction()
endif()
