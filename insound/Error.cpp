#include "Error.h"

#include <stack>
#include <utility>

namespace insound {
    /// Each thread has its own error stack to prevent data races
    thread_local static std::stack<Result> s_errors;    // client error stack
    thread_local static std::stack<Result> s_sysErrors; // used by the program
    static const Result NoErrors {Result::Ok, nullptr};

    static const char *s_codeNames[] = {
        "No errors.",
        "SDL Error",
        "Runtime Error",
        "Logic Error",
        "Invalid Argument",
        "Invalid Handle",
    };

    Result::Result(Code code, const char *message): code(code), message(message)
    { }

    void pushError(Result::Code code, const char *message)
    {
#if INSOUND_DEBUG || INSOUND_LOG_ERRORS
        if (message)
        {
            fprintf(stderr, "INSOUND ERROR: %s: %s\n", s_codeNames[code], s_errors.emplace(code, message).message);
        }
        else
        {
            fprintf(stderr, "INSOUND ERROR: %s\n", s_codeNames[code]);
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
        return s_errors.empty() ? NoErrors : s_sysErrors.top();
    }

    void detail::pushSystemError(Result::Code code, const char *message)
    {
#if INSOUND_DEBUG || INSOUND_LOG_ERRORS
        if (message)
        {
            fprintf(stderr, "INSOUND ERROR: %s: %s\n", s_codeNames[code], s_errors.emplace(code, message).message);
        }
        else
        {
            fprintf(stderr, "INSOUND ERROR: %s\n", s_codeNames[code]);
            s_errors.emplace(code, "");
        }
#else
        s_errors.emplace(code, message ? message : "");
#endif
    }

    bool hasError()
    {
        return !s_errors.empty();
    }
}
