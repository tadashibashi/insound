#pragma once

#define VA_ARGS(...) , ##__VA_ARGS__

#if INSOUND_DEBUG
#ifdef __EMSCRIPTEN__
#include <cstdarg>
#include <cstdio>
#include <emscripten/webaudio.h>

#define INSOUND_LOG(format, ...) do { \
    if (emscripten_current_thread_is_audio_worklet()) { \
        char buffer[512]; \
        std::snprintf(buffer, 512, (format) VA_ARGS(__VA_ARGS__)); \
        EM_ASM({ console.log(UTF8ToString($0)); }, buffer); \
    } else { \
        std::fprintf(stdout, format VA_ARGS(__VA_ARGS__)); \
    } \
} while(0)

#define INSOUND_ERR(format, ...) do { \
    if (emscripten_current_thread_is_audio_worklet()) { \
        char buffer[512]; \
        std::snprintf(buffer, 512, (format) VA_ARGS(__VA_ARGS__)); \
        EM_ASM({ console.error(UTF8ToString($0)); }, buffer); \
    } else { \
        std::fprintf(stderr, (format) VA_ARGS(__VA_ARGS__)); \
    } \
} while(0)


#else // Non-emscripten logging

#define INSOUND_LOG(format, ...) std::fprintf(stdout, (format) VA_ARGS(__VA_ARGS__))
#define INSOUND_ERR(format, ...) std::fprintf(stderr, (format) VA_ARGS(__VA_ARGS__))

#endif


#else // Non debug: strip all logs

#define INSOUND_LOG(format, ...)
#define INSOUND_ERR(format, ...)

#endif // INSOUND_DEBUG
