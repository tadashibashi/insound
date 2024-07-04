#include "PortAudioDevice.h"

#ifdef INSOUND_BACKEND_PORTAUDIO
#include <insound/core/Error.h>
#include <insound/core/util.h>

#include <portaudio.h>

#include <mutex>

#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#endif

namespace insound {
    struct PortAudioDevice::Impl {
        bool paWasInit{};
        PaStream *stream{};
        PaDeviceIndex id{};
        AudioCallback callback{};
        void *userdata{};
        AlignedVector<uint8_t, 16> buffer{};
        AudioSpec spec{};
        std::recursive_mutex mutex{};
        uint32_t requestedBufferFrames{};

        Impl()
        {
            if (const auto err = Pa_Initialize(); err != paNoError)
            {
                INSOUND_PUSH_ERROR(Result::PaErr, Pa_GetErrorText(err));
            }
            else
            {
                paWasInit = true;
            }
        }

        ~Impl()
        {
            if (paWasInit)
            {
                Pa_Terminate();
            }
        }

#ifdef __APPLE__
        static OSStatus deviceChangedListener(AudioObjectID inObjectID, UInt32 inNumberAddresses,
            const AudioObjectPropertyAddress inAddresses[], void *inClientData) {
            // Handle device change
            printf("Default output device has changed.\n");
            auto dev = static_cast<Impl *>(inClientData);
            return dev->refreshDefaultDevice() ? noErr : kAudioHardwareBadDeviceError;
        }
#endif

        bool refreshDefaultDevice()
        {
            std::unique_lock lock(mutex);
            if (this->stream)
            {
                if (const auto result = Pa_StopStream(this->stream); result != paNoError)
                {
                    INSOUND_PUSH_ERROR(Result::PaErr, Pa_GetErrorText(result));
                    return false;
                }
                if (const auto result = Pa_CloseStream(this->stream); result != paNoError)
                {
                    INSOUND_PUSH_ERROR(Result::PaErr, Pa_GetErrorText(result));
                    return false;
                }
                this->stream = nullptr;

                if (const auto result = Pa_Terminate(); result != paNoError)
                {
                    INSOUND_PUSH_ERROR(Result::PaErr, Pa_GetErrorText(result));
                    return false;
                }

                if (const auto result = Pa_Initialize(); result != paNoError)
                {
                    INSOUND_PUSH_ERROR(Result::PaErr, Pa_GetErrorText(result));
                    return false;
                }
            }
            mutex.unlock();

            return open(Pa_GetDefaultOutputDevice(),
                spec.freq, static_cast<int>(requestedBufferFrames),
                callback, userdata);
        }

        bool open(PaDeviceIndex devId, int frequency, int sampleFrameBufferSize, AudioCallback engineCallback, void *userdata)
        {
            std::unique_lock lockGuard(this->mutex);

            frequency = frequency ? frequency : 48000;
            PaStream *stream;
            PaStreamParameters outParams;
            outParams.device = devId;
            outParams.channelCount = 2;
            outParams.sampleFormat = paFloat32;
            outParams.suggestedLatency = 0;
            outParams.hostApiSpecificStreamInfo = nullptr;

            auto err = Pa_OpenStream(&stream, nullptr, &outParams, frequency, sampleFrameBufferSize, 0,
                Impl::paCallback, this);
            if (err != paNoError)
            {
                INSOUND_PUSH_ERROR(Result::PaErr, Pa_GetErrorText(err));
                return false;
            }

            auto id = Pa_GetDefaultOutputDevice();

            if (err = Pa_StartStream(stream); err != paNoError)
            {
                INSOUND_PUSH_ERROR(Result::PaErr, Pa_GetErrorText(err));
                return false;
            }

            lockGuard.unlock();
            close();
            lockGuard.lock();

            this->requestedBufferFrames = sampleFrameBufferSize;
            this->spec.channels = 2;
            this->spec.freq = frequency;
            this->spec.format = SampleFormat(sizeof(float) * CHAR_BIT, true, endian::native == endian::big, true);
            this->callback = engineCallback;
            this->userdata = userdata;
            this->stream = stream;
            this->buffer.resize(sampleFrameBufferSize * sizeof(float) * 2);
            this->id = id;

#ifdef __APPLE__
            // Register device change listener
            AudioObjectPropertyAddress propAddress = {
                kAudioHardwarePropertyDefaultOutputDevice,
                kAudioObjectPropertyScopeGlobal,
                kAudioObjectPropertyElementMain
            };

            AudioObjectAddPropertyListener(kAudioObjectSystemObject, &propAddress, deviceChangedListener, this);
#endif
            return true;
        }

        void close()
        {
            std::lock_guard lockGuard(this->mutex);
            if (this->stream)
            {
#ifdef __APPLE__
                // Register device change listener
                AudioObjectPropertyAddress propAddress = {
                    kAudioHardwarePropertyDefaultOutputDevice,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };

                AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &propAddress, deviceChangedListener, this);
#endif
                Pa_StopStream(this->stream);
                Pa_CloseStream(this->stream);
                this->stream = nullptr;
            }
        }

        static int paCallback(const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData)
        {
            auto dev = static_cast<Impl *>(userData);
            std::lock_guard lockGuard(dev->mutex);

            const auto bufferByteSize = framesPerBuffer * sizeof(float) * 2;

            if (dev->buffer.size() != bufferByteSize)
                dev->buffer.resize(bufferByteSize);
            dev->callback(dev->userdata, &dev->buffer);

            std::memcpy(outputBuffer, dev->buffer.data(), bufferByteSize);
            return 0;
        }
    };

    PortAudioDevice::PortAudioDevice() : m(new Impl)
    { }

    PortAudioDevice::~PortAudioDevice()
    {
        delete m;
    }

    bool PortAudioDevice::open(int frequency, int sampleFrameBufferSize, AudioCallback engineCallback, void *userdata)
    {
        return m->open(Pa_GetDefaultOutputDevice(), frequency, sampleFrameBufferSize, engineCallback, userdata);
    }

    void PortAudioDevice::close()
    {
        m->close();
    }

    void PortAudioDevice::suspend()
    {
        std::lock_guard lockGuard(m->mutex);
        Pa_StopStream(m->stream);
    }

    void PortAudioDevice::resume()
    {
        std::lock_guard lockGuard(m->mutex);
        Pa_StartStream(m->stream);
    }

    bool PortAudioDevice::isOpen() const
    {
        std::lock_guard lockGuard(m->mutex);
        return m->stream != nullptr;
    }

    bool PortAudioDevice::isRunning() const
    {
        std::lock_guard lockGuard(m->mutex);
        return Pa_IsStreamStopped(m->stream);
    }

    uint32_t PortAudioDevice::id() const
    {
        return m->id;
    }

    const AudioSpec & PortAudioDevice::spec() const
    {
        return m->spec;
    }

    int PortAudioDevice::bufferSize() const
    {
        return static_cast<int>(m->buffer.size());
    }

    int PortAudioDevice::getDefaultSampleRate() const
    {
        const auto dev = Pa_GetDefaultOutputDevice();
        const auto info = Pa_GetDeviceInfo(dev);
        return static_cast<int>(info->defaultSampleRate);
    }

} // insound

#endif
