#include "Bus.h"

#include "Command.h"
#include "Engine.h"
#include "Error.h"
#include "SourceRef.h"

namespace insound {
    Bus::Bus(Engine *engine, Handle<Bus> parent, const bool paused) :
        Source(engine, engine && parent && parent.isValid() ? parent->m_clock : 0, paused),
        m_sources(), m_buffer(), m_parent(parent), m_isMaster()
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
        // Only child classes are checked for validity, the Engine caller ensures that the master bus, which begins the
        // recursive call is valid

        Handle<Bus> masterBus;
        if (!engine()->getMasterBus(&masterBus))
        {
            return;
        }

        auto soundEngine = this->engine();

        // Erase-remove idiom on all sound sources with discard flagged true
        m_sources.erase(std::remove_if(m_sources.begin(), m_sources.end(), [&masterBus, soundEngine] (Handle<Source> &handle) {
            if (!handle.isValid())
                return true;
            auto source = handle.get();

            bool shouldDiscard;
            if (!source->shouldDiscard(&shouldDiscard))
                return true; // error, probably due to bad handle, should remove this source

            if (const auto bus = dynamic_cast<Bus *>(source))
            {
                bus->processRemovals(); // if graph is huge, this recursive call could be a problem...

                // if (shouldDiscard) // if this bus is flagged for discarding
                // {
                //     for (auto &subSourceHandle : bus->m_sources)
                //     {
                //         masterBus->applyAppendSource(subSourceHandle);
                //         if (const auto subBus = dynamic_cast<Bus *>(subSourceHandle.get())) // set each subBus's new parent
                //         {
                //             subBus->m_parent = masterBus;
                //         }
                //     }
                // }
            }

            if (shouldDiscard)
                soundEngine->destroySource(handle);
            return shouldDiscard;
        }), m_sources.end());
    }

    void Bus::applyCommand(BusCommand &command)
    {
        // No validation check necessary here, Engine does it in its processCommands function

        switch(command.type)
        {
            case BusCommand::SetOutput:
            {
                auto newParent = command.setoutput.output;

                if (m_parent)
                {
                    m_parent->applyRemoveSource((Handle<Source>)command.bus);
                }

                newParent->applyAppendSource((Handle<Source>)command.bus);
                m_parent = newParent;

            } break;

            case BusCommand::AppendSource:
            {
                applyAppendSource(command.appendsource.source);
            } break;

            case BusCommand::RemoveSource:
            {
                applyRemoveSource(command.removesource.source);
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

    bool Bus::applyAppendSource(Handle<Source> handle)
    {
        m_sources.emplace_back(handle);
        return true;
    }

    bool Bus::applyRemoveSource(Handle<Source> source)
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

            if (dynamic_cast<Bus *>(handle.get()))
            {
                Bus::setOutput((Handle<Bus>)handle, masterBus);
            }
            else
            {
                Bus::appendSource(masterBus, handle);
            }
        }

        return Source::release();
    }

    bool Bus::setOutput(Handle<Bus> bus, Handle<Bus> output)
    {
        if (!bus.isValid())
        {
            pushError(Result::InvalidHandle, "Bus::setOutput: `bus` is invalid");
            return false;
        }

        if (!output.isValid())
        {
            pushError(Result::InvalidHandle, "Bus::setOutput: `output` bus is invalid");
            return false;
        }

        return bus->engine()->pushCommand(Command::makeBusSetOutput(bus, output));
    }

    bool Bus::appendSource(Handle<Bus> bus, Handle<Source> source)
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

        return bus->engine()->pushCommand(Command::makeBusAppendSource(bus, source));
    }

    bool Bus::removeSource(Handle<Bus> bus, Handle<Source> source)
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

        return bus->engine()->pushCommand(Command::makeBusRemoveSource(bus, source));
    }
}
