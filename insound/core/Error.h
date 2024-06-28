#pragma once

namespace insound {
    struct Result {
        enum Code : int {
            Ok = 0,        ///< No errors.
            SdlErr,        ///< SDL runtime error.
            StdExcept,     ///< std::exception thrown
            OutOfMemory,   ///< Ran out of system resources
            RangeErr,
            RuntimeErr,    ///< Insound runtime error.
            LogicErr,      ///< Most likely a user logic error.
            InvalidArg,    ///< Invalid argument passed to a function.
            InvalidHandle, ///< Attempted to use invalid handle.
            EngineNotInit, ///< Engine was not initialized when depended on.
            DecoderNotInit,///< AudioDecoder was not opened when attempted to use it.
            NotSupported,  ///< Feature is not supported.
            FileOpenErr,   ///< Error when attempting to open a file
            InvalidSoundBuffer, ///< SoundBuffer provided was null, or not loaded
            UnexpectedData,///< Unexpected data in buffer, may be due to a malformed file or a bug in the file parsing logic.
            EndOfBuffer,   ///< Buffer attempted to read past end of buffer
            GmeErr,        ///< LibGME runtime error.
            UnknownErr,    ///< Most likely something unexpected was thrown that was not an std::exception
            Count,
        };

        Result(Code code, const char *message);

        Code code;
        const char *message;
    };

    void pushError(Result::Code code, const char *message, const char *functionName, const char *fileName, int lineNumber);

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

#define INSOUND_PUSH_ERROR(code, message) insound::pushError((code), (message), __FUNCTION__, __FILE__, __LINE__);
