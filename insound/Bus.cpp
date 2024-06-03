#include "Bus.h"

#include "Command.h"
#include "Engine.h"
#include "Error.h"
#include "SourceRef.h"

namespace insound {
    Bus::Bus(Engine *engine, Bus *parent, bool paused) : SoundSource(engine, parent ? parent->m_clock : 0, paused),
        m_sources(), m_buffer(), m_parent()
    {
        if (parent)
            setOutputImpl(parent); // defer parent set, since we don't want to append during mix thread, just make sure not to lock before creating this object
    }

    bool Bus::updateParentClock(uint32_t parentClock)
    {
        SoundSource::updateParentClock(parentClock);

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

        BusRef masterBus;
        if (!engine()->getMasterBus(&masterBus))
        {
            return;
        }

        auto soundEngine = this->engine();

        // Erase-remove idiom on all sound sources with discard flagged true
        m_sources.erase(std::remove_if(m_sources.begin(), m_sources.end(), [&masterBus, soundEngine] (SoundSource *source) {
            if (!soundEngine->isSourceValid(source))
                return true;

            bool shouldDiscard;
            if (!source->shouldDiscard(&shouldDiscard))
                return true; // error, probably due to bad handle, should remove this source

            if (const auto bus = dynamic_cast<Bus *>(source))
            {
                bus->processRemovals(); // if graph is huge, this recursive call could be a problem...

                if (shouldDiscard) // if this bus is flagged for discarding
                {
                    for (auto &subSource : bus->m_sources)
                    {
                        masterBus->applyAppendSource(subSource);
                        if (const auto subBus = dynamic_cast<Bus *>(subSource)) // set each subBus's new parent
                        {
                            subBus->m_parent = masterBus.get();
                        }
                    }
                }
            }

            return shouldDiscard;
        }), m_sources.end());
    }

    void Bus::applyCommand(const BusCommand &command)
    {
        // No validation check necessary here, Engine does it in its processCommands function

        switch(command.type)
        {
            case BusCommand::SetOutput:
            {
                const auto newParent = command.setoutput.output;

                if (m_parent)
                {
                    m_parent->applyRemoveSource(this);
                }

                newParent->applyAppendSource(this);
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
            auto source0 = m_sources[sourcei];
            auto source1 = m_sources[sourcei + 1];
            auto source2 = m_sources[sourcei + 2];
            auto source3 = m_sources[sourcei + 3];

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
            auto source0 = m_sources[sourcei];

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
            auto source0 = m_sources[sourcei];

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

    bool Bus::applyAppendSource(SoundSource *source)
    {
        if (!m_engine || !m_engine->isSourceValid(this))
        {
            pushError(Result::InvalidHandle, "Bus");
            return false;
        }

        m_sources.emplace_back(source);
        return true;
    }

    bool Bus::applyRemoveSource(const SoundSource *source)
    {
        if (!m_engine || !m_engine->isSourceValid(this))
        {
            pushError(Result::InvalidHandle, "Bus");
            return false;
        }

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

    bool Bus::release(const bool recursive)
    {
        if (m_isMaster)
        {
            pushError(Result::LogicErr, "Cannot release master bus");
            return false;
        }

        if (detail::popSystemError().code == Result::InvalidHandle)
        {
            pushError(Result::InvalidHandle);
            return false;
        }

        if (recursive)
        {
            if (!SoundSource::release())
            {
                return false;
            }

            // We need to get through all sources, so don't fail on sub-source failure, just flag it and return at end.
            // This lets user know an error occured somewhere, which can be retrieved with `popError`
            bool result = true;
            for (const auto &source : m_sources)
            {
                if (!m_engine->isSourceValid(source))
                    continue; // flag false result here?

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

            return result;
        }

        // non-recursive path
        return release();
    }

    bool Bus::release()
    {
        if (m_isMaster)
        {
            pushError(Result::LogicErr, "Cannot release master bus");
            return false;
        }
        if (detail::popSystemError().code == Result::InvalidHandle)
        {
            pushError(Result::InvalidHandle);
            return false;
        }

        SoundSource::release();

        // only this bus is released => we need to re-attach each sub-sound source to master bus
        BusRef masterBus;
        if (!m_engine->getMasterBus(&masterBus) || !m_engine->isSourceValid(masterBus.get()))
        {
            return false; // failed to get valid master bus
        }

        for (const auto &source : m_sources)
        {
            if (!m_engine->isSourceValid(source))
                continue;

            if (const auto bus = dynamic_cast<Bus *>(source))
            {
                bus->setOutput(masterBus);
            }
            else
            {
                masterBus->appendSourceImpl(source);
            }
        }

        return true;
    }

    bool Bus::setOutput(SourceRef<Bus> output)
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "Bus::setOutput: bus is invalid");
            return false;
        }

        if (!output.isValid())
        {
            pushError(Result::InvalidArg, "Bus::setOutput: `output` bus is invalid");
            return false;
        }

        setOutputImpl(output.get());
        return true;
    }

    bool Bus::appendSource(SoundSourceRef &source)
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "Bus::setOutput: bus is invalid");
            return false;
        }

        if (!source.isValid())
        {
            pushError(Result::InvalidArg, "Bus::appendSource: `source` is invalid");
            return false;
        }

        appendSourceImpl(source.get());
        return true;
    }

    bool Bus::removeSource(SoundSourceRef &source)
    {
        if (detail::peekSystemError().code == Result::InvalidHandle)
        {
            detail::popSystemError();
            pushError(Result::InvalidHandle, "Bus::setOutput: bus is invalid");
            return false;
        }

        if (!source.isValid())
        {
            pushError(Result::InvalidArg, "Bus::removeSource: `source` is invalid");
            return false;
        }

        return engine()->pushCommand(Command::makeBusRemoveSource(this, source.get()));
    }

    std::shared_ptr<Bus> Bus::createMasterBus(Engine *engine)
    {
        auto bus = std::make_shared<Bus>(engine, nullptr, false);
        bus->m_isMaster = true;
        return bus;
    }

    bool Bus::releaseMasterBus(Bus *masterBus)
    {
        if (!masterBus->m_isMaster)
        {
            return false;
        }

        masterBus->m_isMaster = false;
        const auto result = masterBus->release(true);
        if (!result)
        {
            masterBus->m_isMaster = true;
            return false;
        }

        return true;
    }

    bool Bus::setOutputImpl(Bus *output)
    {
        if (output == m_parent) // don't need to update if same object
            return true;

        return engine()->pushCommand(Command::makeBusSetOutput(this, output));
    }

    // caller makes sure that source is already valid
    bool Bus::appendSourceImpl(SoundSource *source)
    {
        if (const auto bus = dynamic_cast<Bus *>(source))
        {
            return bus->setOutputImpl(this);
        }

        return engine()->pushCommand(Command::makeBusAppendSource(this, source));
    }
}
