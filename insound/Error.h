#pragma once

#include <string>

namespace insound {
    struct Error {
        enum Code {
            Ok,
            SdlErr,
            RuntimeErr,
            LogicErr,
            InvalidArg,
        };

        Error(Code code, std::string message);

        Code code;
        std::string message;
    };

    void pushError(Error::Code code, const std::string &message = "");
    Error getError();
}
