#include "Bus.h"

#include "Command.h"
#include "CpuIntrinsics.h"
#include "Engine.h"
#include "Error.h"

namespace insound {
#define HANDLE_GUARD() do { if (detail::peekSystemError().code == Result::InvalidHandle) { \
    detail::popSystemError(); \
    pushError(Result::InvalidHandle, __FUNCTION__); \
    return false; \
} } while(0)

    Bus::Bus() :
        Source(),
        m_sources(), m_buffer(), m_parent(), m_isMaster()
    {}

    Bus::Bus(Bus &&other) noexcept : Source(std::move(other)), m_sources(std::move(other.m_sources)),
        m_buffer(std::move(other.m_buffer)), m_parent(other.m_parent), m_isMaster(other.m_isMaster)
    {}

    bool Bus::updateParentClock(const uint32_t parentClock)
    {
        Source::updateParentClock(parentClock);

        // Recursively update all sub-sources
        uint32_t curClock = m_clock;
        bool result = true;

        for (auto &source : m_sources)
        {
            if (!source->updateParentClock(curClock))
            {
                result = false;
            }
        }

        return result;
    }

    void Bus::processRemovals()
    {
        auto engine = m_engine;

        // Erase-remove idiom on all sound sources with discard flagged true
        m_sources.erase(std::remove_if(m_sources.begin(), m_sources.end(), [engine] (Handle<Source> &handle) {
            if (!handle.isValid())
                return true;
            const auto source = handle.get();

            if (const auto bus = dynamic_cast<Bus *>(source))
                bus->processRemovals(); // if graph is huge, this recursive call could be a problem...

            if (source->shouldDiscard())
            {
                engine->destroySource(handle);
                return true;
            }

            return false;
        }), m_sources.end());
    }

    void Bus::applyCommand(const BusCommand &command)
    {
        // No validation check necessary here, Engine does it in its processCommands function

        switch(command.type)
        {
            case BusCommand::AppendSource:
            {
                auto source = command.appendsource.source;

                // If source is a bus, remove itself from its parent first
                if (auto subBus = source.getAs<Bus>())
                {
                    Handle<Bus> parent;
                    if (subBus->getOutputBus(&parent) && parent.isValid())
                    {
                        parent->applyRemoveSource(source);
                    }

                    subBus->m_parent = command.bus;
                }

                applyAppendSource(source);
            } break;

            case BusCommand::RemoveSource:
            {
                auto source = command.removesource.source;

                // Remove source from this bus
                applyRemoveSource(source);
            } break;

            default:
            {
                pushError(Result::InvalidArg, "Unknown bus command type");
            } break;
        }
    }

    int Bus::readImpl(uint8_t *output, int length)
    {
        // calculate mix
        int sourcei = 0;
        for (const int sourcemax = static_cast<int>(m_sources.size()) - 4; sourcei <= sourcemax; sourcei += 4)
        {
            // Read data from sources (unrolled).
            // note: these should be guaranteed valid because invalidation won't take place until deferred commands
            const auto &sourceA = m_sources[sourcei].get();
            const auto &sourceB = m_sources[sourcei + 1].get();
            const auto &sourceC = m_sources[sourcei + 2].get();
            const auto &sourceD = m_sources[sourcei + 3].get();

            const float *dataA, *dataB, *dataC, *dataD;
            sourceA->read(reinterpret_cast<const uint8_t **>(&dataA), length);
            sourceB->read(reinterpret_cast<const uint8_t **>(&dataB), length);
            sourceC->read(reinterpret_cast<const uint8_t **>(&dataC), length);
            sourceD->read(reinterpret_cast<const uint8_t **>(&dataD), length);

            // Sum each source together with output
            const auto sampleLength = length / sizeof(float);
            auto head = reinterpret_cast<float *>(output);
            int i = 0;
#if INSOUND_SSE
            for (; i <= sampleLength - 16; i += 16)
            {
                const auto a0 = _mm_load_ps(dataA + i);
                const auto a1 = _mm_load_ps(dataA + i + 4);
                const auto a2 = _mm_load_ps(dataA + i + 8);
                const auto a3 = _mm_load_ps(dataA + i + 12);
                const auto b0 = _mm_load_ps(dataB + i);
                const auto b1 = _mm_load_ps(dataB + i + 4);
                const auto b2 = _mm_load_ps(dataB + i + 8);
                const auto b3 = _mm_load_ps(dataB + i + 12);
                const auto c0 = _mm_load_ps(dataC + i);
                const auto c1 = _mm_load_ps(dataC + i + 4);
                const auto c2 = _mm_load_ps(dataC + i + 8);
                const auto c3 = _mm_load_ps(dataC + i + 12);
                const auto d0 = _mm_load_ps(dataD + i);
                const auto d1 = _mm_load_ps(dataD + i + 4);
                const auto d2 = _mm_load_ps(dataD + i + 8);
                const auto d3 = _mm_load_ps(dataD + i + 12);

                const auto sample0 = _mm_add_ps(_mm_add_ps(a0, b0), _mm_add_ps(c0, d0));
                const auto sample1 = _mm_add_ps(_mm_add_ps(a1, b1), _mm_add_ps(c1, d1));
                const auto sample2 = _mm_add_ps(_mm_add_ps(a2, b2), _mm_add_ps(c2, d2));
                const auto sample3 = _mm_add_ps(_mm_add_ps(a3, b3), _mm_add_ps(c3, d3));

                const auto head0 = _mm_load_ps(head + i);
                const auto head1 = _mm_load_ps(head + i + 4);
                const auto head2 = _mm_load_ps(head + i + 8);
                const auto head3 = _mm_load_ps(head + i + 12);

                _mm_store_ps(head + i, _mm_add_ps(head0, sample0));
                _mm_store_ps(head + i + 4, _mm_add_ps(head1, sample1));
                _mm_store_ps(head + i + 8, _mm_add_ps(head2, sample2));
                _mm_store_ps(head + i + 12, _mm_add_ps(head3, sample3));
            }
#elif INSOUND_WASM_SIMD
            for (; i <= sampleLength - 16; i += 16)
            {
                const auto a0 = wasm_v128_load(dataA + i);
                const auto a1 = wasm_v128_load(dataA + i + 4);
                const auto a2 = wasm_v128_load(dataA + i + 8);
                const auto a3 = wasm_v128_load(dataA + i + 12);
                const auto b0 = wasm_v128_load(dataB + i);
                const auto b1 = wasm_v128_load(dataB + i + 4);
                const auto b2 = wasm_v128_load(dataB + i + 8);
                const auto b3 = wasm_v128_load(dataB + i + 12);
                const auto c0 = wasm_v128_load(dataC + i);
                const auto c1 = wasm_v128_load(dataC + i + 4);
                const auto c2 = wasm_v128_load(dataC + i + 8);
                const auto c3 = wasm_v128_load(dataC + i + 12);
                const auto d0 = wasm_v128_load(dataD + i);
                const auto d1 = wasm_v128_load(dataD + i + 4);
                const auto d2 = wasm_v128_load(dataD + i + 8);
                const auto d3 = wasm_v128_load(dataD + i + 12);

                const auto sample0 = wasm_f32x4_add(wasm_f32x4_add(a0, b0), wasm_f32x4_add(c0, d0));
                const auto sample1 = wasm_f32x4_add(wasm_f32x4_add(a1, b1), wasm_f32x4_add(c1, d1));
                const auto sample2 = wasm_f32x4_add(wasm_f32x4_add(a2, b2), wasm_f32x4_add(c2, d2));
                const auto sample3 = wasm_f32x4_add(wasm_f32x4_add(a3, b3), wasm_f32x4_add(c3, d3));

                const auto head0 = wasm_v128_load(head + i);
                const auto head1 = wasm_v128_load(head + i + 4);
                const auto head2 = wasm_v128_load(head + i + 8);
                const auto head3 = wasm_v128_load(head + i + 12);

                wasm_v128_store(head + i, wasm_f32x4_add(head0, sample0));
                wasm_v128_store(head + i + 4, wasm_f32x4_add(head1, sample1));
                wasm_v128_store(head + i + 8, wasm_f32x4_add(head2, sample2));
                wasm_v128_store(head + i + 12, wasm_f32x4_add(head3, sample3));
            }
#elif INSOUND_ARM_NEON
            for (; i <= sampleLength - 16; i += 16)
            {
                const auto a0 = vld1q_f32(dataA + i);
                const auto a1 = vld1q_f32(dataA + i + 4);
                const auto a2 = vld1q_f32(dataA + i + 8);
                const auto a3 = vld1q_f32(dataA + i +  12);
                const auto b0 = vld1q_f32(dataB + i);
                const auto b1 = vld1q_f32(dataB + i + 4);
                const auto b2 = vld1q_f32(dataB + i + 8);
                const auto b3 = vld1q_f32(dataB + i + 12);
                const auto c0 = vld1q_f32(dataC + i);
                const auto c1 = vld1q_f32(dataC + i + 4);
                const auto c2 = vld1q_f32(dataC + i + 8);
                const auto c3 = vld1q_f32(dataC + i + 12);
                const auto d0 = vld1q_f32(dataD + i);
                const auto d1 = vld1q_f32(dataD + i + 4);
                const auto d2 = vld1q_f32(dataD + i + 8);
                const auto d3 = vld1q_f32(dataD + i + 12);

                const auto sample0 = vaddq_f32(vaddq_f32(a0, b0), vaddq_f32(c0, d0));
                const auto sample1 = vaddq_f32(vaddq_f32(a1, b1), vaddq_f32(c1, d1));
                const auto sample2 = vaddq_f32(vaddq_f32(a2, b2), vaddq_f32(c2, d2));
                const auto sample3 = vaddq_f32(vaddq_f32(a3, b3), vaddq_f32(c3, d3));

                const auto head0 = vld1q_f32(head + i);
                const auto head1 = vld1q_f32(head + i + 4);
                const auto head2 = vld1q_f32(head + i + 8);
                const auto head3 = vld1q_f32(head + i + 12);

                vst1q_f32(head + i, vaddq_f32(head0, sample0));
                vst1q_f32(head + i + 4, vaddq_f32(head1, sample1));
                vst1q_f32(head + i + 8, vaddq_f32(head2, sample2));
                vst1q_f32(head + i + 12, vaddq_f32(head3, sample3));
            }
#endif
            // Catch the leftover samples (should be divisible by 2 because of stereo)
            for (; i <= sampleLength - 2; i += 2)
            {
                const auto sample0 = dataA[i] + dataB[i] + dataC[i] + dataD[i];
                const auto sample1 = dataA[i + 1] + dataB[i + 1] + dataC[i + 1] + dataD[i + 1];
                head[i] += sample0;
                head[i + 1] += sample1;
            }
        }
        // Catch the leftover sources
        for (const int sourcecount = (int)m_sources.size(); sourcei < sourcecount; ++sourcei)
        {
            auto source = m_sources[sourcei].get();

            const float *data0;
            auto floatsToRead0 = source->read((const uint8_t **)&data0, length) / sizeof(float);

            auto head = reinterpret_cast<float *>(output);
            for (int i = 0; i < floatsToRead0; i += 4)
            {
                head[i] += data0[i];
                head[i + 1] += data0[i + 1];
                head[i + 2] += data0[i + 2];
                head[i + 3] += data0[i + 3];
            }
        }

        return length;
    }

    bool Bus::applyAppendSource(const Handle<Source> &handle)
    {
        m_sources.emplace_back(handle);
        return true;
    }

    bool Bus::applyRemoveSource(const Handle<Source> &source)
    {
        for (auto it = m_sources.begin(); it != m_sources.end(); ++it)
        {
            if (*it == source)
            {
                m_sources.erase(it);
                return true;
            }
        }

        return false;
    }

    bool Bus::release(bool recursive)
    {
        HANDLE_GUARD();

        if (m_isMaster)
        {
            pushError(Result::LogicErr, "Cannot release master bus");
            return false;
        }

        if (recursive)
        {
            // We need to get through all sources, so don't fail on sub-source failure, just flag it and return at end.
            // This lets user know an error occured somewhere, which can be retrieved with `popError`
            bool result = true;
            for (auto &handle : m_sources)
            {
                if (!handle.isValid())
                    continue; // flag false result here?

                auto source = handle.get();
                if (auto bus = dynamic_cast<Bus *>(source))
                {
                    if (!bus->release(true))
                        result = false;

                }
                else
                {
                    if (!source->release())
                        result = false;
                }
            }

            return Source::release() && result;
        }

        return release(); // non-recursive path
    }

    bool Bus::release()
    {
        HANDLE_GUARD();

        if (m_isMaster)
        {
            pushError(Result::LogicErr, "Cannot release master bus");
            return false;
        }

        // only this bus is released => we need to re-attach each sub-sound source to master bus
        Handle<Bus> masterBus;
        if (!m_engine->getMasterBus(&masterBus) || !masterBus.isValid())
        {
            return false; // failed to get valid master bus
        }

        for (auto &handle : m_sources)
        {
            if (!handle.isValid())
                continue;

            Bus::connect(masterBus, handle);
        }

        return Source::release();
    }

    bool Bus::init(Engine *engine, const Handle<Bus> &parent, bool paused)
    {
        if (!Source::init(engine, engine && parent && parent.isValid() ? parent->m_clock : 0, paused))
            return false;
        m_parent = parent;
        return true;
    }

    bool Bus::connect(const Handle<Bus> &bus, const Handle<Source> &source)
    {
        if (!bus.isValid())
        {
            pushError(Result::InvalidHandle, "Bus::appendSource: `bus` is invalid");
            return false;
        }

        if (!source.isValid())
        {
            pushError(Result::InvalidHandle, "Bus::appendSource: `source` is invalid");
            return false;
        }

        return bus->m_engine->pushCommand(Command::makeBusAppendSource(bus, source));
    }

    bool Bus::disconnect(const Handle<Bus> &bus, const Handle<Source> &source)
    {
        if (!bus.isValid())
        {
            pushError(Result::InvalidHandle, "Bus::removeSoruce: `bus` is invalid");
            return false;
        }

        if (!source.isValid())
        {
            pushError(Result::InvalidArg, "Bus::removeSource: `source` is invalid");
            return false;
        }

        return bus->m_engine->pushCommand(Command::makeBusRemoveSource(bus, source));
    }

    bool Bus::getOutputBus(Handle<Bus> *outBus)
    {
        HANDLE_GUARD();

        if (outBus)
            *outBus = m_parent;
        return true;
    }
}
