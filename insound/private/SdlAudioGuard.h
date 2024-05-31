#pragma once

namespace insound::detail {
    class SdlAudioGuard {
    public:
        SdlAudioGuard();
        ~SdlAudioGuard();

    private:
        bool m_didInit;
        static int s_initCount;
    };
}
