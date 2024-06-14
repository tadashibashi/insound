#pragma once
#include "effects/PanEffect.h"
#include "effects/VolumeEffect.h"
#include "Error.h"

#include <cstdint>
#include <vector>

#include "Engine.h"

namespace insound {
    // Forward declarations
    struct SourceCommand;
    class Engine;
    class Effect;

    struct FadePoint {
        uint32_t clock;
        float value;
    };

    /// Base class for a source that generates an audio signal.
    /// Includes an effects chain, ability to pause/unpause, linear fade points, etc.
    /// To release resources and remove from the mix graph, call `release()`
    ///
    class Source {
    public:
        virtual ~Source() = default;

        /// Get paused status of the sound source
        /// @param outPaused pionter to receive paused state
        /// @returns whether function succeeded; check `popError()` for details; `outPaused` is not mutated on `false`.
        bool getPaused(bool *outPaused) const;

        /// Set the paused status of the sound source
        /// @param value pause state to set
        /// @param clock when to apply pause/unpause in parent clocks; use `getParentClock` to get current parent clock.
        ///              Default value (`UINT32_MAX`) executes pause/unpause right away
        /// @note pause and unpause both can be timed simultaneously, and their values can be updated here.
        ///       set `value` to 0 to clear a timed pause/unpause
        bool setPaused(bool value, uint32_t clock = UINT32_MAX);

        /// Insert effect into effect chain,
        /// @tparam T       the type of effect to initialize
        /// @param position slot in the effect chain; 0 is the first effect and effectCount - 1 is the last
        /// @param args     arguments to initialize the effect with
        /// @returns a pointer to the effect, or `nullptr` if the function failed; check `popError()` for details
        template <typename T, typename ...TArgs>
        Handle<T> addEffect(int position, TArgs &&...args)
        {
            static_assert(std::is_base_of_v<Effect, T>, "`T` must derive from Effect");

            // todo: turn this into a command to send to the engine
            auto lockGuard = m_engine->device().mixLockGuard();

            if (detail::popSystemError().code == Result::InvalidHandle)
            {
                pushError(Result::InvalidHandle, "Source::addEffect");
                return {};
            }

            // Allocate a new effect
            Handle<T> effect;
            try
            {
                effect = m_engine->getObjectPool().allocate<T>(std::forward<TArgs>(args)...);
                if (!effect.isValid())
                {
                    pushError(Result::RuntimeErr, "Out of memory");
                    return {};
                }
            }
            catch(...)
            {
                // constructor threw
                pushError(Result::RuntimeErr, "Effect constructor threw error");
                return {};
            }

            addEffectImpl((Handle<Effect>)effect, position);
            return effect;
        }

        /// Remove an effect from the sound source's effect chain
        /// @param effect effect to remove, this can be retrieved via `getEffect`
        /// @returns whether function succeeded; check `popError` for details; `effect` will not be removed on `false`.
        ///
        /// @note Please take care not to remove the default pan and volume effect, as subsequent access to
        /// `panner()` and `volume()` would result in undefined behavior.
        bool removeEffect(Handle<Effect> effect);

        /// Get an effect from the effect chain
        /// @param position index in the chain where 0 is the start, and `effectCount() - 1` is the last effect
        /// @param outEffect pointer to receive the Effect pointer at the position in the chain.
        ///                  Please make sure position is in range, otherwise access to the pointer will result in
        ///                  undefined behavior.
        /// @return whether function succeeded; check `popError` for details. `outEffect` will not be mutated on `false`.
        bool getEffect(int position, Handle<Effect> *outEffect);
        bool getEffect(int position, Handle<const Effect> *outEffect) const;

        /// Get the number of effects objects in the effect chain
        /// @param outCount pointer to receive the number of effects
        /// @returns whether function succeeded, check `popError` for details. `outCount` will not be mutated on `false`.
        bool getEffectCount(int *outCount) const;

        /// Retrieve the associated Engine object
        bool getEngine(Engine **engine);
        /// Retrieve the associated Engine object
        bool getEngine(const Engine **engine) const;

        /// Get the current clock time in samples
        /// @param outClock pointer to receive the clock time
        /// @returns whether function succeeded, check `popError` for details. `outClock` will not be mutated on `false`.
        bool getClock(uint32_t *outClock) const;

        /// Get the parent bus' clock time in samples
        /// @param outClock pointer to receive the clock time
        /// @returns whether function succeeded, check `popError` for details. `outClock` will not be mutated on `false`.
        [[nodiscard]]
        bool getParentClock(uint32_t *outClock) const;

        /// Get the default panner
        /// @param outPanner [out] pointer to receive the panner handle
        bool getPanner(Handle<PanEffect> *outPanner);
        bool getPanner(Handle<const PanEffect> *outPanner) const;

        /// Get the current volume
        /// @param outVolume pointer to receive volume value where 0 == 0% through 1.f == 100%
        /// @returns whether function succeeded; check `popError` for details. `outVolume` will not be mutated on `false`.
        bool getVolume(float *outVolume) const;

        /// Set the current volume
        /// @param value volume to set e.g. 0 == 0%, 1.f == 100%. Negative values will invert the waveform, values
        ///              higher than 1 are permissible, but beware of clipping.
        /// @returns whether function succeeded; check `popError` for details.
        bool setVolume(float value);

        /// Add a linear fade point. Two must exist for it to be applied.
        /// @param clock parent clock time (samples)
        /// @param value value to fade to
        /// @returns whether function succeeded; check `popError` for details.
        bool addFadePoint(uint32_t clock, float value);

        /// Fade from current value to a target value
        /// @param value  value to fade to
        /// @param length fade time in samples
        /// @returns whether function succeeded; check `popError` for details.
        bool fadeTo(float value, uint32_t length);

        /// Remove fade points between start (inclusive) and end (exclusive), in parent clock cycles
        /// @param start starting point at which to remove fadepoints (inclusive)
        /// @param end   ending point at which to remove fadepoints (up until, but not including);
        ///              default is `UINT32_MAX` which will delete all points after `start`
        /// @returns whether function succeeded; check `popError` for details.
        bool removeFadePoints(uint32_t start, uint32_t end = UINT32_MAX);

        bool getFadeValue(float *outValue) const;

        /// Swap out this sound source's internal output buffer
        bool swapBuffers(std::vector<uint8_t> *buffer);

    protected:
        Source();
        /// All child classes must implement an init function, and call it's parent's init
        bool init(Engine *engine , uint32_t parentClock, bool paused);
        /// Clean up logic before Source's pool memory is deallocated
        virtual bool release();
    private: // private + friend functionality
        friend class Engine;
        friend class Bus;
        friend class MultiPool; // for access to `init` and `release` lifetime functions

        /// Called in `addEffect` to push an add effect command to the Engine. Hides engine implementation.
        Handle<Effect> addEffectImpl(Handle<Effect> effect, int position);

        bool shouldDiscard() const;

        /// Called by Engine when processing commands
        void applyCommand(const SourceCommand &command);
        /// Abstracted into a function for constructor where we immediately add default Pan & Volume to the Source
        void applyAddEffect(const Handle<Effect> &effect, int position);
        void applyAddFadePoint(uint32_t clock, float value);
        void applyRemoveFadePoint(uint32_t startClock, uint32_t endClock);

        /// Engine calls this in the mixer thread to update clock values (called recursively from master bus)
        virtual bool updateParentClock(uint32_t parentClock);

        /// Cause the sound source to populate its output buffer. Optionally get the current pointer position.
        /// @param pcmPtr      pointer to receive pointer to data, may be nullptr
        /// @param length      requested bytes
        /// @returns the amount of bytes available or length arg, whichever is smaller
        int read(const uint8_t **pcmPtr, int length);

        /// Implementation for getting PCM data from the Source
        /// TODO: we only support 32-bit float stereo format, so we may not need to pass units in bytes
        /// @param output pointer to the buffer to fill
        /// @param length size of `output` buffer in bytes
        virtual int readImpl(uint8_t *output, int length) = 0;

    protected:
        // Cached for convenience
        Engine *m_engine;                ///< Reference to the engine for synchronization with the audio thread
        Handle<PanEffect> m_panner;      ///< Default stereo pan effect, second to last in the effect chain
        Handle<VolumeEffect> m_volume;   ///< Default volume effect, last in the effect chain

    private: // member variables
        // Data
        std::vector<Handle<Effect>>m_effects;         ///< Owned audio effects to apply to this source's output
        std::vector<uint8_t> m_outBuffer, m_inBuffer; ///< Temporary buffers to store sound data
        std::vector<FadePoint> m_fadePoints;          ///< Fade points to apply

        // State
        float m_fadeValue;                  ///< Current fade value, multiplied against output (separate from volume)
        uint32_t m_clock, m_parentClock;    ///< Current time in samples since Source and parent was added to the mix graph (check engine spec for sample rate)
        bool m_paused;                      ///< Current pause state, when true, no sound will be output
        int m_pauseClock, m_unpauseClock;   ///< Clock times in samples for timed pauses (check engine spec for sample rate)
        bool m_shouldDiscard;               ///< discard flag, signals the mix graph to remove this object
    };
}
