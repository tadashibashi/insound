#pragma once

namespace insound {
    struct Result {
        enum Code : int {
            Ok = 0,        ///< No errors.
            SdlErr,        ///< SDL runtime error.
            RangeErr,
            RuntimeErr,    ///< Insound runtime error.
            LogicErr,      ///< Most likely a user logic error.
            InvalidArg,    ///< Invalid argument passed to a function.
            InvalidHandle, ///< Attempted to use invalid handle.
            EngineNotInit, ///< Engine was not initialized when depended on.
            NotSupported,  ///< Feature is not supported.
            FileOpenErr,   ///< Error when attempting to open a file
            GmeErr,        ///< LibGME runtime error.
            Count,
        };

        Result(Code code, const char *message);

        Code code;
        const char *message;
    };

    void pushError(Result::Code code, const char *message = nullptr);

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
