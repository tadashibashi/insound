#pragma once
#include <cstdint>

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

    struct Command {
        enum Type {
            EffectParamSet, ///< effect data
            SoundSource,    ///< source data
        } type;

        union {
            struct {
                class IEffect *effect;
                int   index;
                float value;
            } effect;

            struct {
                class ISoundSource *source;
                SourceCommandType::Enum type;
                union {
                    struct {
                        bool value;
                        uint32_t clock;
                    } pause;
                    struct {
                        class IEffect *effect;
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
        } data;

        static Command makeEffectParamSet(class IEffect *effect, const int index, const float value)
        {
            Command c{};
            c.type = EffectParamSet;
            c.data.effect.effect = effect;
            c.data.effect.index = index;
            c.data.effect.value = value;

            return c;
        }

        static Command makeSourcePause(class ISoundSource *source, const bool paused, const uint32_t clock)
        {
            Command c{};
            c.type = SoundSource;
            c.data.source.type = SourceCommandType::SetPause;
            c.data.source.source = source;
            c.data.source.data.pause.value = paused;
            c.data.source.data.pause.clock = clock;
            return c;
        }

        static Command makeSourceEffect(class ISoundSource *source, const bool addEffect, class IEffect *effect, const int position)
        {
            Command c{};
            c.type = SoundSource;
            c.data.source.type = addEffect ? SourceCommandType::AddEffect : SourceCommandType::RemoveEffect;
            c.data.source.source = source;
            c.data.source.data.effect.effect = effect;
            c.data.source.data.effect.position = position;

            return c;
        }

        static Command makeSourceAddFadePoint(class ISoundSource *source, const uint32_t clock, const float value)
        {
            Command c{};
            c.type = SoundSource;
            c.data.source.source = source;
            c.data.source.type = SourceCommandType::AddFadePoint;
            c.data.source.data.addfadepoint.clock = clock;
            c.data.source.data.addfadepoint.value = value;

            return c;
        }

        static Command makeSourceRemoveFadePoint(class ISoundSource *source, const uint32_t begin, const uint32_t end)
        {
            Command c{};
            c.type = SoundSource;
            c.data.source.source = source;
            c.data.source.type = SourceCommandType::RemoveFadePoint;
            c.data.source.data.removefadepoint.begin = begin;
            c.data.source.data.removefadepoint.end = end;

            return c;
        }
    };
}
