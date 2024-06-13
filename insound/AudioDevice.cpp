#include "AudioDevice.h"

#ifdef __EMSCRIPTEN__
#include "EmAudioDevice.h"
#include "SdlAudioDevice.h"
insound::AudioDevice *insound::AudioDevice::create()
{
#ifdef INSOUND_THREADING // Use AudioWorklet backend
    return new EmAudioDevice;
#else                    // Use ScriptProcessorNode backend
    return new SdlAudioDevice;
#endif
}
#else
#include "platform/AudioDevice/SdlAudioDevice.h"
insound::AudioDevice *insound::AudioDevice::create()
{
    return new SdlAudioDevice();
}
#endif

void insound::AudioDevice::destroy(insound::AudioDevice *device)
{
    device->close();
    delete device;
}

#ifdef INSOUND_THREADING
std::lock_guard<std::recursive_mutex> insound::AudioDevice::mixLockGuard()
{
    return std::lock_guard(m_mixMutex);
}
#endif
