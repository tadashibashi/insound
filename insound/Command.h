#pragma once
#include <cstdint>

namespace insound {

    struct SourceCommandType {
        enum Enum {
            SetPause,
            AddEffect,
            RemoveEffect
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
                } data;
            } source;
        } data;

        static Command makeEffectParamSet(class IEffect *effect, int index, float value)
        {
            Command c{};
            c.type = EffectParamSet;
            c.data.effect.effect = effect;
            c.data.effect.index = index;
            c.data.effect.value = value;

            return c;
        }

        static Command makeSourcePause(class ISoundSource *source, bool paused, uint32_t clock)
        {
            Command c{};
            c.type = SoundSource;
            c.data.source.type = SourceCommandType::SetPause;
            c.data.source.source = source;
            c.data.source.data.pause.value = paused;
            c.data.source.data.pause.clock = clock;
            return c;
        }

        static Command makeSourceEffect(class ISoundSource *source, bool addEffect, class IEffect *effect, int position)
        {
            Command c{};
            c.type = SoundSource;
            c.data.source.type = addEffect ? SourceCommandType::AddEffect : SourceCommandType::RemoveEffect;
            c.data.source.source = source;
            c.data.source.data.effect.effect = effect;
            c.data.source.data.effect.position = position;

            return c;
        }
    };
}
