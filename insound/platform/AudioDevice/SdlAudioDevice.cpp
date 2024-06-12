#include "SdlAudioDevice.h"

#include <insound/AudioSpec.h>
#include <insound/Error.h>
#include <insound/platform/getDefaultSampleRate.h>
#include <insound/private/SdlAudioGuard.h>

#include <SDL2/SDL_audio.h>

#include <vector>

#ifdef INSOUND_THREADING
#include <thread>
#endif


namespace insound {
    struct SdlAudioDevice::Impl {
        void (*callback)(void *userdata, std::vector<uint8_t> *stream){};
        void *userData{};

        SDL_AudioDeviceID id{};
        AudioSpec spec{};

#ifdef INSOUND_THREADING
        std::recursive_mutex mixMutex{};
        std::thread mixThread{};
        std::chrono::microseconds threadDelayTarget{};
        bool bufferReadyToQueue{};
#endif

        std::vector<uint8_t> buffer{};
        int bufferSize{};
        detail::SdlAudioGuard m_initGuard{};
    };

    SdlAudioDevice::SdlAudioDevice() : m(new Impl)
    {
    }

    SdlAudioDevice::~SdlAudioDevice()
    {
        delete m;
    }

#ifdef INSOUND_THREADING
    void SdlAudioDevice::mixCallback()
    {
        while (true)
        {
            const auto startTime = std::chrono::high_resolution_clock::now();
            if (m->id == 0)
                break;

            if (isRunning())
            {
                auto lockGuard = std::lock_guard(m_mixMutex);
                if (!m->bufferReadyToQueue)
                {
                    m->callback(m->userData, &m->buffer);
                    m->bufferReadyToQueue = true;
                }

                if ((int)SDL_GetQueuedAudioSize(m->id) - m->bufferSize * 2 <= 0)
                {
                    if (SDL_QueueAudio(m->id, m->buffer.data(), m->bufferSize) != 0)
                    {
                        pushError(Result::SdlErr, SDL_GetError());
                        break;
                    }

                    m->bufferReadyToQueue = false;
                }
            }

            const auto endTime = std::chrono::high_resolution_clock::now();
            std::this_thread::sleep_until(m->threadDelayTarget - (endTime - startTime) + endTime);
        }
    }
#else
    void SdlAudioDevice::audioCallback(void *userdata, uint8_t *stream, int length)
    {
        auto device = (SdlAudioDevice *)(userdata);

        if (device->m->buffer.size() != length)
        {
            device->m->buffer.resize(length, 0);
        }

        device->m->callback(device->m->userData, &device->m->buffer);

        std::memcpy(stream, device->m->buffer.data(), length);
    }
#endif

    bool SdlAudioDevice::open(int frequency, int sampleFrameBufferSize,
        void(*audioCallback)(void *userdata, std::vector<uint8_t> *stream), void *userdata)
    {
        SDL_AudioSpec desired, obtained;

        SDL_memset(&desired, 0, sizeof(desired));
        desired.channels = 2;
        desired.format = AUDIO_F32;
        desired.freq = frequency > 0 ? frequency : getDefaultSampleRate();
        desired.samples = sampleFrameBufferSize;
#ifndef INSOUND_THREADING
        desired.callback = SdlAudioDevice::audioCallback;
        desired.userdata = this;
#endif

        const auto deviceID = SDL_OpenAudioDevice(nullptr, false, &desired, &obtained, 0);
        if (deviceID == 0)
        {
            pushError(Result::SdlErr, SDL_GetError());
            return false;
        }

        close();
        m->spec.channels = obtained.channels;
        m->spec.freq = obtained.freq;
        m->spec.format = SampleFormat(
            SDL_AUDIO_BITSIZE(obtained.format), SDL_AUDIO_ISFLOAT(obtained.format),
            SDL_AUDIO_ISBIGENDIAN(obtained.format), SDL_AUDIO_ISSIGNED(obtained.format)
        );

        m->bufferSize = sampleFrameBufferSize * (int)sizeof(float) * 2;
        m->buffer.resize(m->bufferSize, 0);

        m->id = deviceID;
        m->callback = audioCallback;
        m->userData = userdata;

#ifdef INSOUND_THREADING
        m->threadDelayTarget = std::chrono::microseconds((int)((float)sampleFrameBufferSize / (float)obtained.freq * 500000.f) );
        m->bufferReadyToQueue = false;

        m->mixThread = std::thread([this]() {
            mixCallback();
        });
#endif

        return true;
    }

    void SdlAudioDevice::close()
    {
        if (m->id != 0)
        {
#ifdef INSOUND_THREADING
            const auto deviceID = m->id;
            {
                std::lock_guard lockGuard(m->mixMutex);
                m->id = 0; // flag mix thread to close
            }

            m->mixThread.join();

            SDL_CloseAudioDevice(deviceID);
#else
            SDL_CloseAudioDevice(m->id);
            m->id = 0;
#endif
        }
    }

    void SdlAudioDevice::suspend()
    {
        SDL_PauseAudioDevice(m->id, SDL_TRUE);
    }

    void SdlAudioDevice::resume()
    {
        SDL_PauseAudioDevice(m->id, SDL_FALSE);
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