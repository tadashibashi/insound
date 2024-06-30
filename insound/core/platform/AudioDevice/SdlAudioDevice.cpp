#ifdef INSOUND_BACKEND_SDL2
#include "SdlAudioDevice.h"

#include "../../AudioSpec.h"
#include "../../Error.h"
#include "../../platform/getDefaultSampleRate.h"
#include "../../private/SdlAudioGuard.h"

#include <SDL2/SDL_audio.h>

#include <vector>

#if defined(INSOUND_THREADING) && !defined(__ANDROID__) // Implementation using pthreads
#include <thread>
struct insound::SdlAudioDevice::Impl {
    explicit Impl(SdlAudioDevice *device) : mixMutex(device->m_mixMutex) { }

    bool open(int frequency, int sampleFrameBufferSize,
              AudioCallback audioCallback,
              void *userdata)
    {
        // Set audio spec configuration
        SDL_AudioSpec desired;
        SDL_memset(&desired, 0, sizeof(desired));
        desired.channels = 2;
        desired.format = AUDIO_F32;
        desired.freq = frequency > 0 ? frequency : getDefaultSampleRate();
        if (desired.freq == -1)
            desired.freq = 48000;
        desired.samples = sampleFrameBufferSize;

        // Open device
        SDL_AudioSpec obtained;
        const auto deviceID = SDL_OpenAudioDevice(nullptr, false, &desired, &obtained, 0);
        if (deviceID == 0)
        {
            INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
            return false;
        }

        // Clean up any pre-opened device
        close();

        // Commit values
        spec.channels = obtained.channels;
        spec.freq = obtained.freq;
        spec.format = SampleFormat(
            SDL_AUDIO_BITSIZE(obtained.format), SDL_AUDIO_ISFLOAT(obtained.format),
            SDL_AUDIO_ISBIGENDIAN(obtained.format), SDL_AUDIO_ISSIGNED(obtained.format)
        );

        bufferSize = sampleFrameBufferSize * (int)sizeof(float) * 2; // target buffer size
        buffer.resize(bufferSize /*128 * 2 * sizeof(float)*/, 0);                   // size of our local buffer (matches web audio)

        id = deviceID;
        callback = audioCallback;
        userData = userdata;

        // Start mix thread
        threadDelayTarget = std::chrono::microseconds((int)((float)sampleFrameBufferSize / (float)obtained.freq * 500000.f) );
        mixThread = std::thread(mixCallback, this);

        return true;
    }

    /// Code run in the mix thread. It feeds the SDL audio queue with data it retrieves from the mixed data from the user callback.
    static int mixCallback(void *userptr)
    {
        auto device = (SdlAudioDevice::Impl *)userptr;
        while (true)
        {
            const auto startTime = std::chrono::high_resolution_clock::now();

            if (device->id == 0) // exit if device closes
                break;

            // Calculate mix and queue it if device is playing
            if (auto status = SDL_GetAudioDeviceStatus(device->id); status == SDL_AUDIO_PLAYING)
            {
                auto lockGuard = std::lock_guard(device->mixMutex);

                while ((int)SDL_GetQueuedAudioSize(device->id) - device->bufferSize * 2 <= 0)
                {
                    device->callback(device->userData, &device->buffer);
                    if (SDL_QueueAudio(device->id, device->buffer.data(),(uint32_t)device->buffer.size()) != 0)
                    {
                        INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
                        break;
                    }
                }
            }

            const auto endTime = std::chrono::high_resolution_clock::now();
            std::this_thread::sleep_until(device->threadDelayTarget - (endTime - startTime) + endTime);
        }

        return 0;
    }

    /// Close the sdl audio device. Safe to call if already closed.
    void close()
    {
        if (id != 0)
        {
            const auto deviceID = id;
            {
                std::lock_guard lockGuard(mixMutex);
                id = 0; // flag mix thread to close
            }

            mixThread.join();

            SDL_CloseAudioDevice(deviceID);
        }
    }

    AudioCallback callback{};
    void *userData{};

    SDL_AudioDeviceID id{};
    AudioSpec spec{};

    // Mix thread info
    std::recursive_mutex &mixMutex;
    std::thread mixThread{};
    std::chrono::microseconds threadDelayTarget{};

    // Buffer
    AlignedVector<uint8_t, 16> buffer{};
    int bufferSize{}; // more like the target sdl buffer size

    detail::SdlAudioGuard m_initGuard{};
};

#else  // Non-pthread implementation fallback

struct insound::SdlAudioDevice::Impl {
    Impl(SdlAudioDevice *device) { }

    AudioCallback callback{};
    void *userData{};

    SDL_AudioDeviceID id{};
    AudioSpec spec{};

    /// Open the SDL audio device, setting up the audio callback
    bool open(int frequency, int sampleFrameBufferSize,
              AudioCallback engineCallback,
              void *userdata)
    {
        // Setup configurations
        SDL_AudioSpec desired, obtained;
        SDL_memset(&desired, 0, sizeof(desired));
        desired.channels = 2;
        desired.format = AUDIO_F32;
        desired.freq = frequency > 0 ? frequency : getDefaultSampleRate();
        if (desired.freq == -1)
            desired.freq = 48000;
        desired.samples = sampleFrameBufferSize;
        desired.callback = Impl::audioCallback;
        desired.userdata = this;

        // Open the device
        const auto deviceID = SDL_OpenAudioDevice(nullptr, false, &desired, &obtained, 0);
        if (deviceID == 0)
        {
            INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
            return false;
        }

        // Clean up any pre-existing device in this class
        close();

        // Commit changes
        spec.channels = obtained.channels;
        spec.freq = obtained.freq;
        spec.format = SampleFormat(
            SDL_AUDIO_BITSIZE(obtained.format), SDL_AUDIO_ISFLOAT(obtained.format),
            SDL_AUDIO_ISBIGENDIAN(obtained.format), SDL_AUDIO_ISSIGNED(obtained.format)
        );

        bufferSize = sampleFrameBufferSize * (int)sizeof(float) * 2; // target buffer size
        buffer.resize(bufferSize, 0);                   // TODO: this can potentially result in all buffers being quite large

        id = deviceID;
        callback = engineCallback;
        userData = userdata;

        return true;
    }

    /// SDL audio callback
    static void audioCallback(void *userdata, uint8_t *stream, int length)
    {
        auto device = (SdlAudioDevice::Impl *)(userdata);
        if (device->buffer.size() != length)
        {
            device->buffer.resize(length, 0);
        }

        device->callback(device->userData, &device->buffer);

        std::memcpy(stream, device->buffer.data(), length);
    }

    /// Close the sdl audio device
    void close()
    {
        if (id != 0)
        {
            SDL_CloseAudioDevice(id);
            id = 0;
        }
    }

    AlignedVector<uint8_t, 16> buffer{};
    int bufferSize{};
    detail::SdlAudioGuard m_initGuard{};
};
#endif

namespace insound {
    SdlAudioDevice::SdlAudioDevice() : m(new Impl(this))
    {
    }

    SdlAudioDevice::~SdlAudioDevice()
    {
        delete m;
    }

    bool SdlAudioDevice::open(int frequency, int sampleFrameBufferSize,
                              AudioCallback engineCallback, void *userdata)
    {
        return m->open(frequency, sampleFrameBufferSize, engineCallback, userdata);
    }

    void SdlAudioDevice::close()
    {
        m->close();
    }

    void SdlAudioDevice::suspend()
    {
        SDL_PauseAudioDevice(m->id, SDL_TRUE);
    }

    void SdlAudioDevice::resume()
    {
        SDL_PauseAudioDevice(m->id, SDL_FALSE);
    }

    bool SdlAudioDevice::isOpen() const
    {
        return m->id != 0;
    }

    bool SdlAudioDevice::isRunning() const
    {
        return SDL_GetAudioDeviceStatus(m->id) == SDL_AUDIO_PLAYING;
    }

    uint32_t SdlAudioDevice::id() const
    {
        return m->id;
    }

    const AudioSpec & SdlAudioDevice::spec() const
    {
        return m->spec;
    }

    int SdlAudioDevice::bufferSize() const
    {
        return m->bufferSize;
    }
} // insound
#endif
