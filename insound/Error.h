#pragma once

#include <string>

namespace insound {
    struct Result {
        enum Code {
            Ok,            ///< No errors.
            SdlErr,        ///< SDL runtime error.
            RuntimeErr,    ///< Insound runtime error.
            LogicErr,      ///< Most likely a user logic error.
            InvalidArg,    ///< Invalid argument passed to a function.
            InvalidHandle, ///< Attempted to use invalid handle.
            EngineNotInit,
        };

        Result(Code code, const char *message);

        Code code;
        const char *message;
    };

    void pushError(Result::Code code, const char *message = nullptr);

    [[nodiscard]]
    Result popError();

    [[nodiscard]]
    bool hasError();

    [[nodiscard]]
    const Result &peekError();

    bool lastErrorIs(Result::Code code);

    namespace detail {
        void pushSystemError(Result::Code code, const char *message = nullptr);
        Result popSystemError();
        const Result &peekSystemError();
    }

}
