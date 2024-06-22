#ifdef __EMSCRIPTEN__
#include "EmAudioDevice.h"

#include "../../AudioSpec.h"
#include "../../Error.h"
#include "../../platform/getDefaultSampleRate.h"

#include <emscripten/webaudio.h>

#include <atomic>
#include <thread>
#include <vector>

namespace insound {
    static constexpr int AudioStackSize = 1024 * 1024 * 2; // 2MB

    struct EmAudioDevice::Impl {
        Impl()
        {
            audioThreadStack = (uint8_t *)std::aligned_alloc(16, AudioStackSize);
        }

        ~Impl()
        {
            std::free(audioThreadStack);
        }

        uint8_t *audioThreadStack{};
        AudioCallback callback{};
        void *userData{};

        AudioSpec spec{};

        std::recursive_mutex mixMutex{};
        std::thread mixThread{};
        std::chrono::microseconds threadDelayTarget{};

        AlignedVector<uint8_t, 16> buffer{};
        int bufferSize{};

        EMSCRIPTEN_WEBAUDIO_T context{};
        EMSCRIPTEN_AUDIO_WORKLET_NODE_T audioWorkletNode{};
        std::atomic<int> currentBufferOffset{0};
        std::atomic_bool isRunning{false};
    };

    EmAudioDevice::EmAudioDevice() : m(new Impl)
    {
    }

    EmAudioDevice::~EmAudioDevice()
    {
        delete m;
    }

    int EmAudioDevice::bufferSize() const
    {
        return m->bufferSize;
    }

    static EM_BOOL audioCallback(int numInputs, const AudioSampleFrame *inputs,
                      int numOutputs, AudioSampleFrame *outputs,
                      int numParams, const AudioParamFrame *params,
                      void *userData)
    {
        const auto device = (EmAudioDevice *)userData;

        const auto dataSize = 128 * outputs[0].numberOfChannels * sizeof(float);

        const float *samples;
        device->read(&samples, (int)dataSize);
        for (int i = 0; i < 128; ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                outputs[0].data[j * 128 + i] = samples[i*2 + j];
            }
        }

        return EM_TRUE; // Keep the graph output going
    }

    static EM_BOOL onWindowClick(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData)
    {
        const auto device = (EmAudioDevice *)userData;

        device->resume();

        // Remove this callback
        emscripten_set_click_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, 0, nullptr);
        return EM_FALSE;
    }

    static void audioWorkletProcessorCreated(EMSCRIPTEN_WEBAUDIO_T context, EM_BOOL success, void *userData)
    {
        const auto device = (EmAudioDevice *)userData;
        if (!success)
        {
            pushError(Result::RuntimeErr, "Failed to create audio worklet processor");
            device->close();
            return;
        }

        int outputChannelCounts[] = {2};

        EmscriptenAudioWorkletNodeCreateOptions options = {
            .numberOfInputs = 0,
            .numberOfOutputs = 1,
            .outputChannelCounts = outputChannelCounts
        };

        const auto wasmAudioWorklet = emscripten_create_wasm_audio_worklet_node(context, "insoundMainOutput", &options, &audioCallback, userData);
        device->setAudioWorkletNode((void *)wasmAudioWorklet);

        EM_ASM({
            emscriptenGetAudioObject($0).connect(emscriptenGetAudioObject($1).destination)
        }, wasmAudioWorklet, context);

        emscripten_set_click_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, userData, 0, onWindowClick);
    }

    static void audioThreadInitialized(EMSCRIPTEN_WEBAUDIO_T context, EM_BOOL success, void *userData)
    {
        const auto device = (EmAudioDevice *)userData;
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

    bool EmAudioDevice::open(int frequency, int sampleFrameBufferSize,
        AudioCallback audioCallback, void *userdata)
    {
        // TODO: non-shared-buffer secure context fallback mode? Fallback to SDL device in factory func?

        // Create AudioContext
        EmscriptenWebAudioCreateAttributes attr;
        attr.sampleRate = frequency > 0 ? frequency : getDefaultSampleRate();
        attr.latencyHint = "balanced";
        const auto context = emscripten_create_audio_context(&attr);

        close();
        m->spec.channels = 2;
        m->spec.freq = frequency > 0 ? frequency : getDefaultSampleRate();
        m->spec.format = SampleFormat(sizeof(float) * CHAR_BIT, true, false, true);

        m->bufferSize = sampleFrameBufferSize * (int)sizeof(float) * 2;
        m->buffer.resize(m->bufferSize, 0);

        m->context = context;
        m->callback = audioCallback;
        m->userData = userdata;

        m->currentBufferOffset = m->bufferSize; // triggers first read
        m->threadDelayTarget = std::chrono::microseconds((int)((float)sampleFrameBufferSize / (float)m->spec.freq * 500000.f) );

        emscripten_start_wasm_audio_worklet_thread_async(context, m->audioThreadStack, AudioStackSize,
            &audioThreadInitialized, this);
        return true;
    }

    void EmAudioDevice::close()
    {
        if (m->context != 0)
        {
            std::lock_guard lockGuard(m->mixMutex);

            emscripten_destroy_web_audio_node(m->audioWorkletNode);
            m->audioWorkletNode = 0;

            emscripten_destroy_audio_context(m->context);
            m->context = 0;
        }
    }

    void EmAudioDevice::suspend()
    {
        if (isRunning())
        {
            EM_ASM({
                var audioContext = emscriptenGetAudioObject($0);
                if (audioContext)
                {
                    audioContext.suspend();
                }
            }, m->context);
            m->isRunning.store(false);
        }
    }

    void EmAudioDevice::resume()
    {
        if (!isRunning())
        {
            emscripten_resume_audio_context_sync(m->context);
            m->isRunning.store(true);
        }
    }

    bool EmAudioDevice::isRunning() const
    {
        return emscripten_audio_context_state(m->context) == AUDIO_CONTEXT_STATE_RUNNING;
    }

    bool EmAudioDevice::isPlatformSupported()
    {
        return EM_ASM_INT({
            return !!(window.AudioContext || window.webkitAudioContext) && typeof SharedArrayBuffer === "function";
        });
    }

    bool EmAudioDevice::isOpen() const
    {
        return m->id() != 0;
    }

    uint32_t EmAudioDevice::id() const
    {
        return m->context;
    }

    const AudioSpec &EmAudioDevice::spec() const
    {
        return m->spec;
    }

    void EmAudioDevice::setAudioWorkletNode(void *node)
    {
        if (m->audioWorkletNode)
        {
            emscripten_destroy_web_audio_node(m->audioWorkletNode);
        }

        m->audioWorkletNode = (int)node;
    }

    void EmAudioDevice::read(const float **data, int length)
    {
        if (m->currentBufferOffset >= m->bufferSize)
        {
            m->callback(m->userData, &m->buffer);

            m->currentBufferOffset = 0;
        }

        if (data)
            *data = (float *)(m->buffer.data() + m->currentBufferOffset);
        m->currentBufferOffset.store(m->currentBufferOffset + length);
    }

    void EmAudioDevice::mixCallback()
    {
        while (true)
        {
            const auto start = std::chrono::high_resolution_clock::now();
            if (!isOpen())
                break;
            {
                auto lockGuard = std::lock_guard(m->mixMutex);
                if (m->context == 0)
                    break;

                if (m->currentBufferOffset >= m->bufferSize)
                {
                    m->callback(m->userData, &m->buffer);

                    m->currentBufferOffset = 0;
                }
            }


            const auto stop = std::chrono::high_resolution_clock::now();
            std::this_thread::sleep_until(m->threadDelayTarget - (start - stop) + stop);
        }

    }
} // insound

#endif
