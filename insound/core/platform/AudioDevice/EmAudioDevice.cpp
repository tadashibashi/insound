#ifdef __EMSCRIPTEN__
#include "EmAudioDevice.h"
#include "../../AudioSpec.h"
#include "../../Error.h"
#include "../../platform/getDefaultSampleRate.h"

#include <emscripten/atomic.h>
#include <emscripten/webaudio.h>

#include <cassert>
#include <vector>

namespace insound {
    static constexpr int AudioStackSize = 1024 * 1024 * 2; // 2MB

    struct EmAudioDevice::Impl {
        Impl()
        {
            m_audioThreadStack = (uint8_t *)std::aligned_alloc(16, AudioStackSize);
        }

        ~Impl()
        {
            std::free(m_audioThreadStack);
        }

        [[nodiscard]] bool isBufferReady() const
        {
            return emscripten_atomic_load_u8((const void *)&m_bufferReady);
        }

        [[nodiscard]] bool isRunning() const
        {
            return emscripten_atomic_load_u8((const void *)&m_isRunning);
        }

        bool open(int frequency, int sampleFrameBufferSize,
            AudioCallback audioCallback, void *userdata)
        {
            // Create AudioContext
            EmscriptenWebAudioCreateAttributes attr;
            attr.sampleRate = frequency > 0 ? frequency : getDefaultSampleRate();
            attr.latencyHint = "interactive";
            const auto context = emscripten_create_audio_context(&attr);

            close();
            m_spec.channels = 2;
            m_spec.freq = frequency > 0 ? frequency : getDefaultSampleRate();
            m_spec.format = SampleFormat(sizeof(float) * CHAR_BIT, true, false, true);

            m_bufferSize = sampleFrameBufferSize * (int)sizeof(float) * 2;
            assert(sampleFrameBufferSize >= 128 * 2); // should b e>= than emscripten's minimum callback byte size
            m_buffer.resize(m_bufferSize, 0);
            m_nextBuffer.resize(m_bufferSize, 0);

            m_context = context;
            m_callback = audioCallback;
            m_userData = userdata;

            m_curBufferOffset = 0;
            m_bufferReady = false;
            m_isRunning = false;

            emscripten_start_wasm_audio_worklet_thread_async(context, m_audioThreadStack, AudioStackSize,
                &audioThreadInitialized, this);
            return true;
        }

        void close()
        {
            if (m_context != 0)
            {
                emscripten_destroy_web_audio_node(m_audioWorkletNode);
                m_audioWorkletNode = 0;

                emscripten_destroy_audio_context(m_context);
                m_context = 0;
            }
        }

        void suspend()
        {
            if (isRunning())
            {
                EM_ASM({
                    var audioContext = emscriptenGetAudioObject($0);
                    if (audioContext)
                    {
                        audioContext.suspend();
                    }
                }, m_context);
                setIsRunning(false);
            }
        }

        void resumeAsync()
        {
            emscripten_resume_audio_context_async(m_context, audioContextResumed, this);
        }

        void resume()
        {
            emscripten_resume_audio_context_sync(m_context);
            setIsRunning(true);
        }

        void read(const float **data, int length)
        {
            if (!isRunning())
            {
                *data = (float *)m_buffer.data();
                return;
            }

            if (!isBufferReady()) // process next buffer
            {
                m_callback(m_userData, &m_nextBuffer);
                setBufferReady(true);
            }

            if (m_curBufferOffset >= m_bufferSize)
            {
                if (isBufferReady())
                {
                    m_buffer.swap(m_nextBuffer);
                    m_curBufferOffset = 0;
                    setBufferReady(false); // signal the mix thread to start processing next buffer
                }
                else
                {
                    // oh no, buffer starvation :(
                    *data = (float *)m_buffer.data();
                    return;
                }
            }

            *data = reinterpret_cast<float *>(m_buffer.data() + m_curBufferOffset);
            m_curBufferOffset += length;
        }

        [[nodiscard]]
        const AudioSpec &spec() const { return m_spec; }

        [[nodiscard]]
        auto id() const { return m_context; }

        [[nodiscard]]
        auto bufferSize() const { return m_bufferSize; }

    private:
        uint8_t *m_audioThreadStack{};
        AudioCallback m_callback{};
        void *m_userData{};

        AudioSpec m_spec{};
        AlignedVector<uint8_t, 16> m_buffer{}, m_nextBuffer{};
        int m_bufferSize{};

        EMSCRIPTEN_WEBAUDIO_T m_context{0};
        EMSCRIPTEN_AUDIO_WORKLET_NODE_T m_audioWorkletNode{0};
        int m_curBufferOffset{0};

        volatile uint8_t m_isRunning{0};
        volatile uint8_t m_bufferReady{0};

        void setBufferReady(bool value)
        {
            emscripten_atomic_store_u8((void *)&m_bufferReady, static_cast<bool>(value));
        }

        void setIsRunning(bool value)
        {
            emscripten_atomic_store_u8((void *)&m_isRunning, static_cast<bool>(value));
        }

        // WebAudio-related callbacks
        static void audioWorkletProcessorCreated(EMSCRIPTEN_WEBAUDIO_T context, EM_BOOL success, void *userData);
        static void audioThreadInitialized(EMSCRIPTEN_WEBAUDIO_T context, EM_BOOL success, void *userData);
        static EM_BOOL onWindowClick(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData);
        static EM_BOOL emAudioCallback(int numInputs, const AudioSampleFrame *inputs,
            int numOutputs, AudioSampleFrame *outputs,
            int numParams, const AudioParamFrame *params,
            void *userData);
        static void audioContextResumed(EMSCRIPTEN_WEBAUDIO_T audioContext, AUDIO_CONTEXT_STATE state, void *userData1);
    };

    void EmAudioDevice::read(const float **data, int length)
    {
        m->read(data, length);
    }

    EmAudioDevice::EmAudioDevice() : m(new Impl)
    {
    }

    EmAudioDevice::~EmAudioDevice()
    {
        delete m;
    }

    bool EmAudioDevice::open(int frequency, int sampleFrameBufferSize,
        AudioCallback audioCallback, void *userdata)
    {
        return m->open(frequency, sampleFrameBufferSize, audioCallback, userdata);
    }

    void EmAudioDevice::close()
    {
        m->close();
    }

    void EmAudioDevice::suspend()
    {
        m->suspend();
    }

    void EmAudioDevice::resume()
    {
        m->resumeAsync();
    }

    int EmAudioDevice::bufferSize() const
    {
        return m->bufferSize();
    }

    bool EmAudioDevice::isRunning() const
    {
        return m->isRunning();
    }

    bool EmAudioDevice::isPlatformSupported()
    {
        return EM_ASM_INT({
            return !!(window.AudioContext || window.webkitAudioContext) &&
                typeof SharedArrayBuffer === "function";
        });
    }

    bool EmAudioDevice::isOpen() const
    {
        return m->id() != 0;
    }

    uint32_t EmAudioDevice::id() const
    {
        return static_cast<uint32_t>(m->id());
    }

    const AudioSpec &EmAudioDevice::spec() const
    {
        return m->spec();
    }

    // ===== Static WebAudio Callbacks ================================================================================

    /// Callback for user interaction workaround with web audio
    EM_BOOL EmAudioDevice::Impl::onWindowClick(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData)
    {
        const auto device = static_cast<EmAudioDevice::Impl *>(userData);

        device->resumeAsync();

        // Remove this callback
        emscripten_set_click_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, 0, nullptr);
        return EM_FALSE;
    }

    /// Called when audio thread is initialized
    /// @param context associated web audio context
    /// @param success whether audio thread was created successfully
    /// @param userData pointer castable to EmAudioDevice::Impl *
    void EmAudioDevice::Impl::audioThreadInitialized(EMSCRIPTEN_WEBAUDIO_T context, EM_BOOL success, void *userData)
    {
        const auto device = static_cast<EmAudioDevice::Impl *>(userData);
        if (!success)
        {
            pushError(Result::RuntimeErr, "Failed to init audio thread");
            device->close();
            return;
        }

        constexpr WebAudioWorkletProcessorCreateOptions opts = {
            .name = "insoundMainOutput",
        };
        emscripten_create_wasm_audio_worklet_processor_async(context, &opts, &audioWorkletProcessorCreated, userData);
    }

    /// Called when audio worklet processor finishes creating
    /// @param context associated audio context
    /// @param success whether creation succeeded
    /// @param userdata pointer castable to EmAudioDevice::Impl *
    void EmAudioDevice::Impl::audioWorkletProcessorCreated(EMSCRIPTEN_WEBAUDIO_T context, EM_BOOL success, void *userData)
    {
        const auto device = static_cast<EmAudioDevice::Impl *>(userData);
        if (!success)
        {
            pushError(Result::RuntimeErr, "Failed to create audio worklet processor");
            device->close();
            return;
        }

        // Set up configurations for AudioWorklet
        int outputChannelCounts[] = {2};
        EmscriptenAudioWorkletNodeCreateOptions options = {
            .numberOfInputs = 0,
            .numberOfOutputs = 1,
            .outputChannelCounts = outputChannelCounts
        };

        // Create the AudioWorklet node
        const auto wasmAudioWorklet = emscripten_create_wasm_audio_worklet_node(context, "insoundMainOutput", &options,
            &emAudioCallback, userData);

        // Connect AudioWorklet to AudioContext's hardware output
        EM_ASM({
            emscriptenGetAudioObject($0).connect(emscriptenGetAudioObject($1).destination)
        }, wasmAudioWorklet, context);

        // Setup user interaction workaround
        emscripten_set_click_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, userData, 0, onWindowClick);

        // Done! Commit changes to the device
        if (device->m_audioWorkletNode != NULL)
            emscripten_destroy_web_audio_node(device->m_audioWorkletNode);
        device->m_audioWorkletNode = wasmAudioWorklet;
    }

    EM_BOOL EmAudioDevice::Impl::emAudioCallback(int numInputs, const AudioSampleFrame *inputs,
        int numOutputs, AudioSampleFrame *outputs,
        int numParams, const AudioParamFrame *params,
        void *userData)
    {
        /// Assert engine format requirement
        assert(numOutputs > 0 && outputs[0].numberOfChannels == 2);
        constexpr auto dataSize = 128 * 2 * sizeof(float);

        const auto device = static_cast<EmAudioDevice::Impl *>(userData);
        if (!device->isRunning())
        {
            return EM_TRUE;
        }

        // Read samples from the mixer
        const float *samples{};
        device->read(&samples, (int)dataSize);
        assert(samples);

        // Reformat the samples from interleaved to block
        // [LRLR...] -> [LL...RR...]
        const auto output = outputs[0].data;
        for (int i = 0; i < 128; i += 4)
        {
            const auto ix2 = i * 2;
            const auto ip128 = i + 128;
            output[i]         = samples[ix2];
            output[ip128]     = samples[ix2 + 1];
            output[i + 1]     = samples[ix2 + 2];
            output[ip128 + 1] = samples[ix2 + 3];
            output[i + 2]     = samples[ix2 + 4];
            output[ip128 + 2] = samples[ix2 + 5];
            output[i + 3]     = samples[ix2 + 6];
            output[ip128 + 3] = samples[ix2 + 7];
        }

        return EM_TRUE; // Keep the graph output going
    }

    // Callback handling async audio context resume
    void EmAudioDevice::Impl::audioContextResumed(EMSCRIPTEN_WEBAUDIO_T audioContext, AUDIO_CONTEXT_STATE state, void *userData1)
    {
        const auto device = static_cast<EmAudioDevice::Impl *>(userData1);
        if (state == AUDIO_CONTEXT_STATE_RUNNING)
            device->setIsRunning(true);
    }

} // insound

#endif
