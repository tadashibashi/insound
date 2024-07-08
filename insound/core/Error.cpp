#include "Error.h"
#include "logging.h"
#include <stack>
#include <utility>

namespace insound {
    /// Each thread has its own error stack to prevent data races
    thread_local static std::stack<Result> s_errors;    // client error stack
    thread_local static std::stack<Result> s_sysErrors; // used by the program
    static const Result NoErrors {Result::Ok, nullptr};
    constexpr int MAX_ERR_STACK_SIZE = 32;
    static const char *s_codeNames[] = {
        "No errors",
        "SDL Error",
        "PortAudio Error",
        "MiniAudio Error",
        "Standard exception was thrown",
        "Ran out of system memory",
        "Out of range",
        "Runtime Error",
        "Logic Error",
        "Invalid Argument",
        "Invalid Handle",
        "Engine uninitialized",
        "AudioDecoder unitialized",
        "Stream uninitialized",
        "Feature unsupported",
        "File failed to open",
        "SoundBuffer was null or not loaded",
        "Unexpected data found in buffer",
        "Attempted to read past end of buffer",
        "LibGME Runtime Error",
        "Unknown error",
    };
    static_assert(Result::Count == sizeof(s_codeNames) / sizeof(char *));

    Result::Result(Code code, const char *message): code(code), message(message)
    { }

    void pushError(Result::Code code, const char *message, const char *functionName, const char *fileName, int lineNumber)
    {
        if (s_sysErrors.size() >= MAX_ERR_STACK_SIZE)
            return;
#if INSOUND_DEBUG || INSOUND_LOGGING
        if (code >= Result::Count)
            code = (Result::Code)(Result::Count - 1);
        else if (code < 0)
            code = (Result::Code)0;

        if (message)
        {
            INSOUND_ERR("INSOUND ERROR: in %s:%d: %s: %s: %s\n",
                fileName, lineNumber, functionName, s_codeNames[code],
                s_errors.emplace(code, message).message);
        }
        else
        {
            INSOUND_ERR("INSOUND ERROR: in %s:%d: %s: %s\n", fileName, lineNumber, functionName,
                s_codeNames[code]);
            s_errors.emplace(code, "");
        }
#else
        s_errors.emplace(code, message ? message : "");
#endif
    }

    Result popError()
    {
        if (s_errors.empty())
        {
            return NoErrors;
        }

        const auto err = s_errors.top();
        s_errors.pop();

        return err;
    }

    const Result &peekError()
    {
        return (s_errors.empty()) ? NoErrors : s_errors.top();
    }

    bool lastErrorIs(Result::Code code)
    {
        return peekError().code == code;
    }

    Result detail::popSystemError()
    {
        if (s_sysErrors.empty())
        {
            return NoErrors;
        }

        const auto &err = s_sysErrors.top();
        s_sysErrors.pop();

        return err;
    }

    const Result &detail::peekSystemError()
    {
        return s_sysErrors.empty() ? NoErrors : s_sysErrors.top();
    }

    void detail::pushSystemError(Result::Code code, const char *message)
    {
        if (s_sysErrors.size() >= MAX_ERR_STACK_SIZE)
            return;
        if (code >= Result::Count)
            code = (Result::Code)(Result::Count - 1);
        else if (code < 0)
            code = (Result::Code)0;
#if 0 //INSOUND_DEBUG || INSOUND_LOG_ERRORS


        if (message)
        {
            //fprintf(stderr, "INSOUND ERROR: %s: %s\n", s_codeNames[code],
                s_sysErrors.emplace(code, message);
        }
        else
        {
            //fprintf(stderr, "INSOUND ERROR: %s\n", s_codeNames[code]);
            s_sysErrors.emplace(code, "");
        }
#else
        s_sysErrors.emplace(code, message ? message : "");
#endif
    }

    bool hasError()
    {
        return !s_errors.empty();
    }
}
