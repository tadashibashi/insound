/// Logging macros for Insound Core
/// Make sure not to put code with side-effects into this macro, as all statements will
/// be stripped in Release mode when INSOUND_LOGGING is not defined.
#pragma once

#define VA_ARGS(...) , ##__VA_ARGS__

#if defined(INSOUND_DEBUG) || defined(INSOUND_LOGGING)
#ifdef __EMSCRIPTEN__      // HTML browser logging
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


#elif defined(__ANDROID__) // Android logging
#include <android/log.h>

#define INSOUND_LOG(format, ...) __android_log_print(ANDROID_LOG_INFO, "Insound", (format) VA_ARGS(__VA_ARGS__))
#define INSOUND_ERR(format, ...) __android_log_print(ANDROID_LOG_ERROR, "Insound", (format) VA_ARGS(__VA_ARGS__))

#else                      // Desktop, iOS logging
#include <cstdio>
#define INSOUND_LOG(format, ...) std::fprintf(stdout, (format) VA_ARGS(__VA_ARGS__))
#define INSOUND_ERR(format, ...) std::fprintf(stderr, (format) VA_ARGS(__VA_ARGS__))

#endif


#else // Non logging environments: strip all logs

#include <insound/lib.h>

#define INSOUND_LOG(format, ...) INSOUND_NOOP
#define INSOUND_ERR(format, ...) INSOUND_NOOP

#endif // INSOUND_DEBUG
