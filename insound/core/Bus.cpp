#include "Bus.h"

#include "Command.h"
#include "Engine.h"
#include "Error.h"
#include "SourceRef.h"

namespace insound {
    Bus::Bus() :
        Source(),
        m_sources(), m_buffer(), m_parent(), m_isMaster()
    {
    }

    bool Bus::updateParentClock(uint32_t parentClock)
    {
        Source::updateParentClock(parentClock);

        // Recursively update all sub-sources
        uint32_t curClock;
        if (!getClock(&curClock))
        {
            return false;
        }

        for (auto &source : m_sources)
        {
            if (!source->updateParentClock(curClock))
            {
                return false;
            }
        }

        return true;
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

                // // Re-attach source to master bus
                // if (!m_isMaster)
                // {
                //     if (auto subBus = source.getAs<Bus>())
                //     {
                //         Handle<Bus> masterBus;
                //         if (m_engine->getMasterBus(&masterBus))
                //         {
                //             masterBus->applyAppendSource(source);
                //             subBus->m_parent = masterBus;
                //         }
                //     }
                // }


            } break;

            default:
            {
                pushError(Result::InvalidArg, "Unknown bus command type");
            } break;
        }
    }

    int Bus::readImpl(uint8_t *output, int length)
    {
        // resize buffer if necessary
        if (m_buffer.size() * sizeof(float) < length)
        {
            m_buffer.resize(length / sizeof(float));
        }

        // clear buffer to 0s
        std::memset(m_buffer.data(), 0, m_buffer.size() * sizeof(float));

#if 1 // efficient path
        // calculate mix
        int sourcei = 0;
        for (int sourcecount = (int)m_sources.size() - 4; sourcei < sourcecount; sourcei += 4)
        {
            auto source0 = m_sources[sourcei].get();
            auto source1 = m_sources[sourcei + 1].get();
            auto source2 = m_sources[sourcei + 2].get();
            auto source3 = m_sources[sourcei + 3].get();

            const float *data0, *data1, *data2, *data3;
            auto floatsToRead0 = source0->read((const uint8_t **)&data0, length) / sizeof(float);
            auto floatsToRead1 = source1->read((const uint8_t **)&data1, length) / sizeof(float);
            auto floatsToRead2 = source2->read((const uint8_t **)&data2, length) / sizeof(float);
            auto floatsToRead3 = source3->read((const uint8_t **)&data3, length) / sizeof(float);

            float *head = m_buffer.data();
            for (int i = 0; i < floatsToRead0; i += 4)
            {
                head[i] += data0[i];
                head[i + 1] += data0[i + 1];
                head[i + 2] += data0[i + 2];
                head[i + 3] += data0[i + 3];
            }
            for (int i = 0; i < floatsToRead1; i += 4)
            {
                head[i] += data1[i];
                head[i + 1] += data1[i + 1];
                head[i + 2] += data1[i + 2];
                head[i + 3] += data1[i + 3];
            }
            for (int i = 0; i < floatsToRead2; i += 4)
            {
                head[i] += data2[i];
                head[i + 1] += data2[i + 1];
                head[i + 2] += data2[i + 2];
                head[i + 3] += data2[i + 3];
            }
            for (int i = 0; i < floatsToRead3; i += 4)
            {
                head[i] += data3[i];
                head[i + 1] += data3[i + 1];
                head[i + 2] += data3[i + 2];
                head[i + 3] += data3[i + 3];
            }
        }
        for (int sourcecount = (int)m_sources.size(); sourcei < sourcecount; ++sourcei)
        {
            auto source0 = m_sources[sourcei].get();

            const float *data0, *data1, *data2, *data3;
            auto floatsToRead0 = source0->read((const uint8_t **)&data0, length) / sizeof(float);

            float *head = m_buffer.data();
            for (int i = 0; i < floatsToRead0; i += 4)
            {
                head[i] += data0[i];
                head[i + 1] += data0[i + 1];
                head[i + 2] += data0[i + 2];
                head[i + 3] += data0[i + 3];
            }
        }
#else
        for (int sourcei = 0, sourcecount = (int)m_sources.size(); sourcei < sourcecount; ++sourcei)
        {
            auto source0 = m_sources[sourcei].get<Source>();

            const float *data0, *data1, *data2, *data3;
            auto floatsToRead0 = source0->read((const uint8_t **)&data0, length) / sizeof(float);

            float *head = m_buffer.data();
            for (int i = 0; i < floatsToRead0; i += 4)
            {
                head[i] += data0[i];
                head[i + 1] += data0[i + 1];
                head[i + 2] += data0[i + 2];
                head[i + 3] += data0[i + 3];
            }
        }
#endif

        std::memcpy(output, m_buffer.data(), length);
        return (int)(m_buffer.size() * sizeof(float));
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
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle);
            return false;
        }

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
        if (detail::popSystemError().code == Result::InvalidHandle)
        {
            pushError(Result::InvalidHandle);
            return false;
        }

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
        if (detail::popSystemError().code == Result::InvalidHandle)
        {
            pushError(Result::InvalidHandle);
            return false;
        }

        if (outBus)
            *outBus = m_parent;
        return true;
    }
}
