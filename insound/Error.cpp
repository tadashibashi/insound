#include "Error.h"

#include <stack>
#include <utility>

namespace insound {
    static std::stack<Error> s_errors;

    Error::Error(Code code, std::string message): code(code), message(std::move(message))
    { }

    void pushError(Error::Code code, const std::string &message)
    {
#if INSOUND_DEBUG || INSOUND_LOG_ERRORS
        fprintf(stderr, "MACH ERROR: %s\n", s_errors.emplace(code, message).message.c_str());
#else
        s_errors.emplace(code, message);
#endif
    }

    Error getError()
    {
        if (s_errors.empty())
        {
            return {Error::Ok, "No errors."};
        }

        auto err = s_errors.top();
        s_errors.pop();

        return err;
    }
}
