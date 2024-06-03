#pragma once
#include <cstdint>

namespace insound {
    // ======  Command types ==================================================
    struct EffectCommand {
        class Effect *effect;

        enum Type {
            SetFloat,
            SetInt,
            SetString,
        } type;

        union {
            struct {
                int index;
                float value;
            } setfloat;

            struct {
                int index;
                int value;
            } setint;

            struct {
                int index;
                const char *value;
            } setstring;
        };
    };

    struct EngineCommand {
        /// The Engine to perform this command on
        class Engine *engine;

        /// Command subtype
        enum Type {
            ReleaseSource,
        } type;

        /// Data union for the various commands set by `type`
        union {
            struct {
                class SoundSource *source;
            } releasesource;
        };
    };

    struct SoundSourceCommand {
        /// SoundSource to perform this command on
        class SoundSource *source;

        /// Command subtype
        enum Type {
           SetPause,
           AddEffect,
           RemoveEffect,
           AddFadePoint,
           RemoveFadePoint,
        } type;

        /// Data union for the various commands set by `type`
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
        };
    };

    struct BusCommand {
        /// The bus to perform this command on
        class Bus *bus;

        /// Command subtype
        enum Type {
            SetOutput,
            AppendSource,
            RemoveSource,
        } type;

        /// Data union for the various commands set by `type`
        union{
            struct {
                class Bus *output;
            } setoutput;
            struct {
                class SoundSource *source;
            } appendsource;
            struct {
                class SoundSource *source;
            } removesource;
        };
    };

    struct PCMSourceCommand {
        /// PCMSource to perform this command on
        class PCMSource *source;

        /// Command subtype
        enum Type {
            SetPosition,
            SetSpeed,
        } type;

        /// Data union for the various commands set by `type`
        union{
            struct {
                float position;
            } setposition;

            struct {
                float speed;
            } setspeed;
        };
    };

    // ======  Main command struct ============================================
    struct Command {
        enum Type {
            Effect,         ///< `effect` data
            Engine,         ///< `engine` data
            SoundSource,    ///< `source` data
            PCMSource,      ///< `pcmsource` data
            Bus,            ///< `bus` data
        } type;

        union {
            EffectCommand      effect;
            EngineCommand      engine;
            SoundSourceCommand source;
            PCMSourceCommand   pcmsource;
            BusCommand         bus;
        };

        // ====== Static helpers =============================================

        static Command makeBusSetOutput(class Bus *bus, class Bus *output)
        {
            Command c{};
            c.type = Type::Bus;
            c.bus.bus = bus;
            c.bus.type = BusCommand::SetOutput;
            c.bus.setoutput.output = output;

            return c;
        }

        static Command makeBusAppendSource(class Bus *bus, class SoundSource *source)
        {
            Command c{};
            c.type = Type::Bus;
            c.bus.bus = bus;
            c.bus.type = BusCommand::AppendSource;
            c.bus.appendsource.source = source;

            return c;
        }

        static Command makeBusRemoveSource(class Bus *bus, class SoundSource *source)
        {
            Command c{};
            c.type = Type::Bus;
            c.bus.bus = bus;
            c.bus.type = BusCommand::RemoveSource;
            c.bus.appendsource.source = source;

            return c;
        }

        static Command makeEngineReleaseSource(class Engine *engine, class SoundSource *source)
        {
            Command c{};
            c.type = Command::Engine,
            c.engine.engine = engine;
            c.engine.type = EngineCommand::ReleaseSource;
            c.engine.releasesource.source = source;

            return c;
        }
        static Command makeEffectSetFloat(class Effect *effect, const int index, const float value)
        {
            Command c{};
            c.type = Effect;
            c.effect.effect = effect;
            c.effect.setfloat.index = index;
            c.effect.setfloat.value = value;

            return c;
        }
        static Command makeEffectSetInt(class Effect *effect, const int index, const int value)
        {
            Command c{};
            c.type = Effect;
            c.effect.effect = effect;
            c.effect.setint.index = index;
            c.effect.setint.value = value;

            return c;
        }
        static Command makeEffectSetString(class Effect *effect, const int index, const char *value)
        {
            Command c{};
            c.type = Effect;
            c.effect.effect = effect;
            c.effect.setstring.index = index;
            c.effect.setstring.value = value;

            return c;
        }

        static Command makeSourcePause(class SoundSource *source, const bool paused, const uint32_t clock)
        {
            Command c{};
            c.type = SoundSource;
            c.source.type = SoundSourceCommand::SetPause;
            c.source.source = source;
            c.source.pause.value = paused;
            c.source.pause.clock = clock;
            return c;
        }

        static Command makeSourceEffect(class SoundSource *source, const bool addEffect, class Effect *effect, const int position)
        {
            Command c{};
            c.type = SoundSource;
            c.source.type = addEffect ? SoundSourceCommand::AddEffect : SoundSourceCommand::RemoveEffect;
            c.source.source = source;
            c.source.effect.effect = effect;
            c.source.effect.position = position;

            return c;
        }

        static Command makeSourceAddFadePoint(class SoundSource *source, const uint32_t clock, const float value)
        {
            Command c{};
            c.type = SoundSource;
            c.source.source = source;
            c.source.type = SoundSourceCommand::AddFadePoint;
            c.source.addfadepoint.clock = clock;
            c.source.addfadepoint.value = value;

            return c;
        }

        static Command makeSourceRemoveFadePoint(class SoundSource *source, const uint32_t begin, const uint32_t end)
        {
            Command c{};
            c.type = SoundSource;
            c.source.source = source;
            c.source.type = SoundSourceCommand::RemoveFadePoint;
            c.source.removefadepoint.begin = begin;
            c.source.removefadepoint.end = end;

            return c;
        }

        static Command makePCMSourceSetPosition(class PCMSource *source, const float position)
        {
            Command c{};
            c.type = PCMSource;
            c.pcmsource.type = PCMSourceCommand::SetPosition;
            c.pcmsource.source = source;
            c.pcmsource.setposition.position = position;

            return c;
        }

        static Command makePCMSourceSetSpeed(class PCMSource *source, const float speed)
        {
            Command c{};
            c.type = PCMSource;
            c.pcmsource.type = PCMSourceCommand::SetSpeed;
            c.pcmsource.source = source;
            c.pcmsource.setspeed.speed = speed;

            return c;
        }
    };
}
