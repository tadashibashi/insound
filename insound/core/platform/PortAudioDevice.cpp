#include "PortAudioDevice.h"

#ifdef INSOUND_BACKEND_PORTAUDIO
#include <insound/core/lib.h>
#include <insound/core/Error.h>
#include <insound/core/util.h>

#include <portaudio.h>

#include <mutex>

#if INSOUND_TARGET_MACOS
#include <CoreAudio/CoreAudio.h>
#endif

#if INSOUND_TARGET_WINDOWS
#include <mmdeviceapi.h>
#endif

namespace insound {
    struct PortAudioDevice::Impl {
#if INSOUND_TARGET_WINDOWS
        class DeviceChangeNotification : public IMMNotificationClient {
        public:
            explicit DeviceChangeNotification(Impl *m) : m(m) { }

        private:
            // Implement necessary methods from IMMNotificationClient interface
            STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId) override {
                // Handle default device change
                printf("Device changed\n");
                // Implement your logic here
                auto lock = std::lock_guard(m->mutex);
                m->id = -1;
                return S_OK;
            }

            // Implement other methods like OnDeviceAdded, OnDeviceRemoved as needed

            // Other methods like OnDeviceStateChanged, OnPropertyValueChanged can also be implemented

            PortAudioDevice::Impl *m;

            // Inherited via IMMNotificationClient
            HRESULT __stdcall QueryInterface(REFIID riid, void **ppvObject) override
            {
                return E_NOTIMPL;
            }
            ULONG __stdcall AddRef(void) override
            {
                return 0;
            }
            ULONG __stdcall Release(void) override
            {
                return 0;
            }
            HRESULT __stdcall OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override
            {
                return 0;
            }
            HRESULT __stdcall OnDeviceAdded(LPCWSTR pwstrDeviceId) override
            {
                return 0;
            }
            HRESULT __stdcall OnDeviceRemoved(LPCWSTR pwstrDeviceId) override
            {
                return 0;
            }
            HRESULT __stdcall OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override
            {
                return 0;
            }
        } devNotificationClient{this};
        IMMDeviceEnumerator *devEnumerator{};
#endif

        bool paWasInit{};
        std::atomic<PaStream *> stream{};
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
#if INSOUND_TARGET_WINDOWS
                auto result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                    __uuidof(IMMDeviceEnumerator), (void **)&devEnumerator);
                if (FAILED(result))
                {
                    INSOUND_PUSH_ERROR(Result::RuntimeErr, "MMDevice enumerator failed to create");
                    return;
                }

                result = devEnumerator->RegisterEndpointNotificationCallback(&devNotificationClient);
                if (FAILED(result))
                {
                    INSOUND_PUSH_ERROR(Result::RuntimeErr, "MMDevice enumerator failed to register callback");
                    devEnumerator->Release();
                }
#endif
            }
        }

        ~Impl()
        {
            if (paWasInit)
            {
                Pa_Terminate();
            }
        }

#if INSOUND_TARGET_APPLE
        static OSStatus deviceChangedListener(AudioObjectID inObjectID, UInt32 inNumberAddresses,
            const AudioObjectPropertyAddress inAddresses[], void *inClientData) {
            auto dev = static_cast<Impl *>(inClientData);
            auto lockGuard = std::lock_guard(dev->mutex);
            dev->id = -1;
            return noErr;
        }
#endif

        bool refreshDefaultDevice()
        {
            const auto stream = this->stream.load(std::memory_order_acquire);
            if (stream)
            {
                if (auto result = Pa_StopStream(stream); result != paNoError && result != paStreamIsStopped)
                {
                    result = Pa_AbortStream(stream);
                    if (result != paNoError)
                        INSOUND_PUSH_ERROR(Result::PaErr, Pa_GetErrorText(result));
                }
                this->stream.store(nullptr, std::memory_order_release);

                if (const auto result = Pa_Terminate(); result != paNoError) // Terminate auto-closes all opened streams
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

            return open(Pa_GetDefaultOutputDevice(),
                spec.freq, static_cast<int>(requestedBufferFrames),
                callback, userdata);
        }

        bool open(PaDeviceIndex devId, int frequency, int sampleFrameBufferSize, AudioCallback engineCallback, void *userdata)
        {
            std::unique_lock lockGuard(this->mutex);

            frequency = frequency ? frequency : 48000;
            PaStream *stream;
            PaStreamParameters outParams{};
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

#if INSOUND_TARGET_APPLE
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
            auto stream = this->stream.load(std::memory_order_acquire);
            if (stream)
            {
#if INSOUND_TARGET_APPLE
                // Register device change listener
                AudioObjectPropertyAddress propAddress = {
                    kAudioHardwarePropertyDefaultOutputDevice,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };

                AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &propAddress, deviceChangedListener, this);
#endif

#if INSOUND_TARGET_WINDOWS
                if (devEnumerator)
                {
                    devEnumerator->UnregisterEndpointNotificationCallback(&devNotificationClient);
                    devEnumerator->Release();
                    devEnumerator = nullptr;
                }
#endif
                Pa_StopStream(stream);
                Pa_CloseStream(stream);
                this->stream.store(nullptr, std::memory_order_release);
            }
        }

        static int paCallback(const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData)
        {
            const auto dev = static_cast<Impl *>(userData);

            if (!dev->mutex.try_lock())
            {
                return 0;
            }

            try {
               const auto bufferByteSize = framesPerBuffer * sizeof(float) * 2;

                if (dev->buffer.size() != bufferByteSize)
                    dev->buffer.resize(bufferByteSize);
                dev->callback(dev->userdata, &dev->buffer);

                std::memcpy(outputBuffer, dev->buffer.data(), bufferByteSize);
            }
            catch (...)
            {
                dev->mutex.unlock();
                return 0;
            }

            dev->mutex.unlock();
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
        Pa_StopStream(m->stream.load(std::memory_order_acquire));
    }

    void PortAudioDevice::resume()
    {
        Pa_StartStream(m->stream.load(std::memory_order_acquire));
    }

    bool PortAudioDevice::isOpen() const
    {
        return m->stream.load(std::memory_order_acquire) != nullptr;
    }

    bool PortAudioDevice::isRunning() const
    {
        return Pa_IsStreamStopped(m->stream.load(std::memory_order_acquire));
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

    void PortAudioDevice::update()
    {
        std::lock_guard lock(m->mutex);
        if (m->id != Pa_GetDefaultOutputDevice())
        {
            m->refreshDefaultDevice();
        }
    }

} // insound

#endif

