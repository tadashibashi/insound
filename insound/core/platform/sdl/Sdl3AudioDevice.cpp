#ifdef INSOUND_BACKEND_SDL3
#include "Sdl3AudioDevice.h"
#include "Sdl3AudioGuard.h"

#include <insound/core/AudioSpec.h>
#include <insound/core/Error.h>

#include <SDL3/SDL_audio.h>

#if defined(INSOUND_THREADING) && !defined(__ANDROID__) // Implementation using pthreads
#include <thread>

struct insound::Sdl3AudioDevice::Impl {
    explicit Impl(Sdl3AudioDevice *device) : mixMutex(device->m_mixMutex) { }
    static void sdl3AudioCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
    {
        auto dev = static_cast<Sdl3AudioDevice::Impl *>(userdata);
        if (dev->buffer.size() < additional_amount)
            dev->buffer.resize(additional_amount);
        dev->callback(dev->userData, &dev->buffer);
        SDL_PutAudioStreamData(stream, dev->buffer.data(), dev->buffer.size());
    }
    bool open(int frequency, int sampleFrameBufferSize,
              AudioCallback audioCallback,
              void *userdata)
    {
        // Set audio spec configuration
        SDL_AudioSpec desired{};
        desired.channels = 2;
        desired.format = SDL_AUDIO_F32;
        desired.freq = frequency > 0 ? frequency : getDefaultSampleRate();
        if (desired.freq == -1)
            desired.freq = 48000;
        // Open device
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &desired, sdl3AudioCallback, this);
        if (!stream)
        {
            INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
            return false;
        }

        // Clean up any pre-opened device
        // close();

        // // Commit values
        // SDL_AudioSpec obtained;
        // int sampleBufferFrames;
        // if (SDL_GetAudioDeviceFormat(deviceID, &obtained, &sampleBufferFrames) != 0)
        // {
        //     INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
        //     SDL_CloseAudioDevice(deviceID);
        //     return false;
        // }


        // auto stream = SDL_CreateAudioStream(&desired, &desired);
        // if (!stream)
        // {
        //     INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
        //     SDL_CloseAudioDevice(deviceID);
        //     return false;
        // }
        //
        // if (SDL_BindAudioStream(deviceID, stream) != 0)
        // {
        //     INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
        //     SDL_CloseAudioDevice(deviceID);
        //     return false;
        // }

        spec.channels = desired.channels;
        spec.freq = desired.freq;
        spec.format = SampleFormat(
            SDL_AUDIO_BITSIZE(desired.format), SDL_AUDIO_ISFLOAT(desired.format),
            SDL_AUDIO_ISBIGENDIAN(desired.format), SDL_AUDIO_ISSIGNED(desired.format)
        );

        bufferSize = sampleFrameBufferSize * (int)sizeof(float) * 2; // target buffer size
        buffer.resize(bufferSize /*128 * 2 * sizeof(float)*/, 0);                   // size of our local buffer (matches web audio)

        //id = deviceID;
        callback = audioCallback;
        userData = userdata;
        //this->stream = stream;

        // Start mix thread TODO: use conditional instead of arbitarary sleep period
        //threadDelayTarget = std::chrono::microseconds((int)((float)sampleFrameBufferSize / (float)obtained.freq * 500000.f) );
        //mixThread = std::thread(mixCallback, this);
        SDL_ResumeAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT);
        return true;
    }

    /// Code run in the mix thread. It feeds the SDL audio queue with data it retrieves from the mixed data from the user callback.
    static int mixCallback(void *userptr)
    {
        auto device = (Sdl3AudioDevice::Impl *)userptr;
        while (true)
        {
            const auto startTime = std::chrono::high_resolution_clock::now();

            if (device->id == 0) // exit if device closes
                break;

            // Calculate mix and queue it if device is playing
            if (!SDL_AudioDevicePaused(device->id))
            {
                auto lockGuard = std::lock_guard(device->mixMutex);

                while ((int)SDL_GetAudioStreamAvailable(device->stream) - device->bufferSize * 2 <= 0)
                {
                    device->callback(device->userData, &device->buffer);
                    if (SDL_PutAudioStreamData(device->stream, device->buffer.data(),(uint32_t)device->buffer.size()) != 0)
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

    SDL_AudioStream *stream;

    // Buffer
    AlignedVector<uint8_t, 16> buffer{};
    int bufferSize{}; // more like the target sdl buffer size

    detail::Sdl3AudioGuard m_initGuard{};
};

#else  // Non-pthread implementation fallback

struct insound::Sdl3AudioDevice::Impl {
    Impl(Sdl3AudioDevice *device) { }

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
        auto device = (Sdl3AudioDevice::Impl *)(userdata);
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
    detail::Sdl3AudioGuard m_initGuard{};
};
#endif

namespace insound {
    Sdl3AudioDevice::Sdl3AudioDevice() : m(new Impl(this))
    {
    }

    Sdl3AudioDevice::~Sdl3AudioDevice()
    {
        delete m;
    }

    bool Sdl3AudioDevice::open(int frequency, int sampleFrameBufferSize,
                              AudioCallback engineCallback, void *userdata)
    {
        return m->open(frequency, sampleFrameBufferSize, engineCallback, userdata);
    }

    void Sdl3AudioDevice::close()
    {
        m->close();
    }

    void Sdl3AudioDevice::suspend()
    {
        SDL_PauseAudioDevice(m->id);
    }

    void Sdl3AudioDevice::resume()
    {
        SDL_ResumeAudioDevice(m->id);
    }

    bool Sdl3AudioDevice::isOpen() const
    {
        return m->id != 0;
    }

    bool Sdl3AudioDevice::isRunning() const
    {
        return !SDL_AudioDevicePaused(m->id);
    }

    uint32_t Sdl3AudioDevice::id() const
    {
        return m->id;
    }

    const AudioSpec & Sdl3AudioDevice::spec() const
    {
        return m->spec;
    }

    int Sdl3AudioDevice::bufferSize() const
    {
        return m->bufferSize;
    }

    int Sdl3AudioDevice::getDefaultSampleRate() const
    {
        SDL_AudioSpec defaultSpec;
        if (SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &defaultSpec, nullptr) != 0)
        {
            INSOUND_PUSH_ERROR(Result::SdlErr, SDL_GetError());
            return -1;
        }

        return defaultSpec.freq;
    }

} // insound
#endif
