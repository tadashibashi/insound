#pragma once

namespace insound {
    class Bus;
    class PCMSource;
    class SoundSource;

    template <typename T>
    class SourceRef;

    using BusRef = SourceRef<Bus>;
    using PCMSourceRef = SourceRef<PCMSource>;
    using SoundSourceRef = SourceRef<SoundSource>;
}
