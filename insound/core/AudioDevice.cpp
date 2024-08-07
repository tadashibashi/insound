#include "AudioDevice.h"
#include "lib.h"

#if INSOUND_TARGET_EMSCRIPTEN
#   include "platform/EmAudioDevice.h"

    insound::AudioDevice *insound::AudioDevice::create()
    {
#   ifdef INSOUND_THREADING // Use AudioWorklet backend
        return new EmAudioDevice();
#   else                    // Use ScriptProcessorNode backend
#error "Non-threaded emscripten backend platform not supported yet"
    // TODO: implement ScriptProcessorNode or other non-thread backend
#   endif
    }
#elif INSOUND_TARGET_ANDROID
#   include "platform/android/AAudioDevice.h"
    insound::AudioDevice *insound::AudioDevice::create()
    {
        return new AAudioDevice();
    }
#elif INSOUND_TARGET_IOS
#   include "platform/ios/iOSAudioDevice.h"
insound::AudioDevice *insound::AudioDevice::create()
{
    return new iOSAudioDevice();
}

#elif INSOUND_TARGET_DESKTOP && defined(INSOUND_BACKEND_PORTAUDIO)
#include "platform/PortAudioDevice.h"
insound::AudioDevice *insound::AudioDevice::create()
    {
        return new PortAudioDevice();
    }
#elif defined(INSOUND_BACKEND_SDL2)
#   include "platform/sdl/SdlAudioDevice.h"
    insound::AudioDevice *insound::AudioDevice::create()
    {
        return new SdlAudioDevice();
    }
#elif defined(INSOUND_BACKEND_SDL3)
#   include "platform/sdl/Sdl3AudioDevice.h"
    insound::AudioDevice *insound::AudioDevice::create()
    {
        return new Sdl3AudioDevice();
    }
#endif

void insound::AudioDevice::destroy(insound::AudioDevice *device)
{
    device->close();
    delete device;
}
