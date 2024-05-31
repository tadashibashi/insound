#pragma once
#include <cstdint>

#include "PCMSource.h"

namespace insound {

    struct SourceCommandType {
        enum Enum {
            SetPause,
            AddEffect,
            RemoveEffect,
            AddFadePoint,
            RemoveFadePoint,
        };
    };

    struct BusCommandType {
        enum Enum {
            SetOutput,
        };
    };

    struct PCMSourceCommandType {
        enum Enum {
            SetPosition
        };
    };

    struct Command {
        enum Type {
            EffectParamSet, ///< effect data
            SoundSource,    ///< source data
            PCMSource,      ///< pcmsource data
            Bus,
        } type;

        union {
            struct {
                class Effect *effect;
                int   index;
                float value;
            } effect;

            struct {
                class SoundSource *source;
                SourceCommandType::Enum type;
                union {
                    struct {
                        bool value;
                        uint32_t clock;
                    } pause;
                    struct {
                        class Effect *effect;
                        int position;
                    } effect;
                    struct {
                        uint32_t clock;
                        float value;
                    } addfadepoint;
                    struct {
                        uint32_t begin;
                        uint32_t end;
                    } removefadepoint;
                } data;
            } source;

            struct {
                class PCMSource *source;
                PCMSourceCommandType::Enum type;
                union {
                    struct {
                        uint32_t position;
                    } setposition;
                } data;
            } pcmsource;

            struct {
                class Bus *bus;
                BusCommandType::Enum type;
                union {
                    struct {
                        class Bus *output;
                    } setoutput;
                } data;
            } bus;
        } data;

        static Command makeEffectParamSet(class Effect *effect, const int index, const float value)
        {
            Command c{};
            c.type = EffectParamSet;
            c.data.effect.effect = effect;
            c.data.effect.index = index;
            c.data.effect.value = value;

            return c;
        }

        static Command makeSourcePause(class SoundSource *source, const bool paused, const uint32_t clock)
        {
            Command c{};
            c.type = SoundSource;
            c.data.source.type = SourceCommandType::SetPause;
            c.data.source.source = source;
            c.data.source.data.pause.value = paused;
            c.data.source.data.pause.clock = clock;
            return c;
        }

        static Command makeSourceEffect(class SoundSource *source, const bool addEffect, class Effect *effect, const int position)
        {
            Command c{};
            c.type = SoundSource;
            c.data.source.type = addEffect ? SourceCommandType::AddEffect : SourceCommandType::RemoveEffect;
            c.data.source.source = source;
            c.data.source.data.effect.effect = effect;
            c.data.source.data.effect.position = position;

            return c;
        }

        static Command makeSourceAddFadePoint(class SoundSource *source, const uint32_t clock, const float value)
        {
            Command c{};
            c.type = SoundSource;
            c.data.source.source = source;
            c.data.source.type = SourceCommandType::AddFadePoint;
            c.data.source.data.addfadepoint.clock = clock;
            c.data.source.data.addfadepoint.value = value;

            return c;
        }

        static Command makeSourceRemoveFadePoint(class SoundSource *source, const uint32_t begin, const uint32_t end)
        {
            Command c{};
            c.type = SoundSource;
            c.data.source.source = source;
            c.data.source.type = SourceCommandType::RemoveFadePoint;
            c.data.source.data.removefadepoint.begin = begin;
            c.data.source.data.removefadepoint.end = end;

            return c;
        }

        static Command makePCMSourceSetPosition(class PCMSource *source, const uint32_t position)
        {
            Command c{};
            c.type = Type::PCMSource;
            c.data.pcmsource.type = PCMSourceCommandType::SetPosition;
            c.data.pcmsource.source = source;
            c.data.pcmsource.data.setposition.position = position;

            return c;
        }

        static Command makeBusSetOutput(class Bus *bus, class Bus *output)
        {
            Command c{};
            c.type = Type::Bus;
            c.data.bus.bus = bus;
            c.data.bus.type = BusCommandType::SetOutput;
            c.data.bus.data.setoutput.output = output;

            return c;
        }
    };
}
