#ifdef __ANDROID__
#include "AAudioDevice.h"

#include <insound/core/Error.h>
#include <insound/core/platform/android/AndroidNative.h>
#include <aaudio/AAudio.h>
#include <mutex>

namespace insound {
    struct AAudioDevice::Impl {
        AAudioStream *m_stream{};
        AudioCallback m_callback{};
        void *m_userData{};
        AudioSpec m_spec{};
        AlignedVector<uint8_t, 16> m_buffer{};
        mutable std::recursive_mutex m_mutex{};

        bool open(int frequency, int sampleFrameBufferSize, AudioCallback audioCallback,
             void *userdata)
        {
            std::lock_guard lockGuard(m_mutex);

            AAudioStreamBuilder *builder;
            if (const auto result = AAudio_createStreamBuilder(&builder); result != AAUDIO_OK)
            {
                INSOUND_PUSH_ERROR(Result::RuntimeErr, AAudio_convertResultToText(result));
                return false;
            }

            if (frequency == 0)
                frequency = 48000;

            AAudioStreamBuilder_setSampleRate(builder, frequency);
            AAudioStreamBuilder_setBufferCapacityInFrames(builder,
                                                          sampleFrameBufferSize == 0 ? 512 : sampleFrameBufferSize);
            AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED); // low latency
            AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
            AAudioStreamBuilder_setChannelCount(builder, 2);
#if __ANDROID_MIN_SDK_VERSION__ >= 32 // channel mask only available in 32+
            AAudioStreamBuilder_setChannelMask(builder, AAUDIO_CHANNEL_STEREO);
#endif
            AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
            AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
            AAudioStreamBuilder_setDataCallback(builder, aaudioCallback, this);

            m_spec.channels = 2;
            m_spec.freq = frequency;
            m_spec.format = SampleFormat(sizeof(float) * CHAR_BIT, true, false, true);
            m_userData = userdata;
            m_callback = audioCallback;
            if (auto result = AAudioStreamBuilder_openStream(builder, &m_stream); result != AAUDIO_OK)
            {
                AAudioStreamBuilder_delete(builder);
                INSOUND_PUSH_ERROR(Result::RuntimeErr, AAudio_convertResultToText(result));
                return false;
            }

            AAudioStreamBuilder_delete(builder);

            if (auto result = AAudioStream_requestStart(m_stream); result != AAUDIO_OK)
            {
                AAudioStream_close(m_stream);
                return false;
            }
            // TODO: double check that we're 2-channel float?
            return true;
        }

        void suspend()
        {
            std::lock_guard lockGuard(m_mutex);
            if (m_stream)
                AAudioStream_requestPause(m_stream);
        }

        void resume()
        {
            std::lock_guard lockGuard(m_mutex);
            if (m_stream)
                AAudioStream_requestStart(m_stream);
        }

        void close()
        {
            std::lock_guard lockGuard(m_mutex);
            if (m_stream)
            {
                AAudioStream_requestStop(m_stream);
                AAudioStream_close(m_stream);
                m_stream = nullptr;
            }
        }

        [[nodiscard]]
        bool isRunning() const
        {
            std::lock_guard lockGuard(m_mutex);
            if (!m_stream)
                return false;
            return AAudioStream_getState(m_stream) == AAUDIO_STREAM_STATE_STARTED;
        }

        [[nodiscard]]
        uint32_t id() const
        {
            std::lock_guard lockGuard(m_mutex);
            if (!m_stream)
                return 0;
            return AAudioStream_getDeviceId(m_stream);
        }

        [[nodiscard]]
        int bufferSize() const
        {
            std::lock_guard lockGuard(m_mutex);
            if (!m_stream)
                return 0;
            return AAudioStream_getBufferCapacityInFrames(m_stream) *
                   m_spec.channels *
                   (m_spec.format.bits() / CHAR_BIT);
        }

        [[nodiscard]]
        bool isOpen() const
        {
            std::lock_guard lockGuard(m_mutex);
            return m_stream != nullptr;
        }

        static aaudio_data_callback_result_t aaudioCallback(AAudioStream *stream,
                                                            void *userData,
                                                            void *audioData,
                                                            int32_t nFrames)
        {
            auto device = static_cast<AAudioDevice::Impl *>(userData);
            std::lock_guard lockGuard(device->m_mutex);
            if (AAudioStream_getState(device->m_stream) != AAUDIO_STREAM_STATE_STARTED)
            {
                return AAUDIO_CALLBACK_RESULT_CONTINUE;
            }

            const auto byteSize = nFrames * sizeof(float) * 2;
            if (device->m_buffer.size() != byteSize)
                device->m_buffer.resize(byteSize);

            device->m_callback(device->m_userData, &device->m_buffer);
            std::memcpy(audioData, device->m_buffer.data(), byteSize);

            return AAUDIO_CALLBACK_RESULT_CONTINUE;
        }
    };

    AAudioDevice::AAudioDevice() : m(new Impl)
    { }

    AAudioDevice::~AAudioDevice()
    {
        delete m;
    }


    bool AAudioDevice::open(int frequency, int sampleFrameBufferSize,
                            insound::AudioCallback audioCallback, void *userdata)
    {
        return m->open(frequency, sampleFrameBufferSize, audioCallback, userdata);
    }

    void AAudioDevice::suspend()
    {
        m->suspend();
    }

    void AAudioDevice::resume()
    {
        m->resume();
    }

    void AAudioDevice::close()
    {
        m->close();
    }

    bool AAudioDevice::isRunning() const
    {
        return m->isRunning();
    }

    uint32_t AAudioDevice::id() const
    {
        return m->id();
    }

    const AudioSpec &AAudioDevice::spec() const
    {
        return m->m_spec;
    }

    int AAudioDevice::bufferSize() const
    {
        return m->bufferSize();
    }

    bool AAudioDevice::isOpen() const
    {
        return m->isOpen();
    }

    int AAudioDevice::getDefaultSampleRate() const
    {
        return getAndroidDefaultSampleRate();
    }
} // insound

#endif
