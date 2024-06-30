#include "AudioDevice.h"
#include "lib.h"

#if INSOUND_TARGET_EMSCRIPTEN
#include "platform/AudioDevice/EmAudioDevice.h"
#include "platform/AudioDevice/SdlAudioDevice.h"

insound::AudioDevice *insound::AudioDevice::create()
{
#ifdef INSOUND_THREADING // Use AudioWorklet backend
    return new EmAudioDevice();
#else                    // Use ScriptProcessorNode backend
    return new SdlAudioDevice;
#endif
}
#elif INSOUND_TARGET_ANDROID
#include "platform/AudioDevice/AAudioDevice.h"
//#include "platform/AudioDevice/SdlAudioDevice.h"
insound::AudioDevice *insound::AudioDevice::create()
{
    return new AAudioDevice();
}

#else

#if defined(INSOUND_BACKEND_SDL2)
#include "platform/AudioDevice/SdlAudioDevice.h"
insound::AudioDevice *insound::AudioDevice::create()
{
    return new SdlAudioDevice();
}
#endif

#endif

void insound::AudioDevice::destroy(insound::AudioDevice *device)
{
    device->close();
    delete device;
}
