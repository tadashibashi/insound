#pragma once

namespace insound {
    struct EffectCommand;
    class Engine;

    /// Base class for an audio effect, which is insertable into any Source object
    class Effect {
    public:
        virtual ~Effect() = default;
    protected:
        Effect() : m_engine() { }
        Effect(Effect &&other) noexcept;

        /// Set a floating point parameter value
        /// @param index id of the parameter to send
        /// @param value float parameter to send
        bool sendFloat(int index, float value);
        /// Set an integer parameter value
        /// @param index id of the parameter to send
        /// @param value int parameter to send
        bool sendInt(int index, int value);
        /// Set a string parameter value
        /// @param index id of the parameter to send
        /// @param value string parameter to send
        bool sendString(int index, const char *value);

        [[nodiscard]]
        Engine *engine() { return m_engine; }
        [[nodiscard]]
        const Engine *engine() const { return m_engine; }

        /// Clean up logic
        virtual void release() { }
    private:
        friend class Engine;
        friend class Source;
        friend class MultiPool; // for access to `init` and `release` lifetime functions
        void applyCommand(const EffectCommand &command);

        /// Override this if you need to process float parameter sets.
        /// Don't set member parameters directly as it may occur during audio thread,
        /// set via `send*` functions and apply changes in `receive*` functions.
        /// @param index index passed in `sendFloat` now received
        /// @param value value passed in `sendFloat` now received
        virtual void receiveFloat(int index, float value) {}

        /// Override this if you need to process integer parameter sets.
        /// Don't set member parameters directly as it may occur during audio thread,
        /// set via `sendInt` functions and apply changes in this function.
        /// @param index index passed in `sendInt` now received
        /// @param value value passed in `sendInt` now received
        virtual void receiveInt(int index, int value) {}

        /// Override this if you need to process string parameter sets.
        /// Don't set member parameters directly as it may occur during audio thread,
        /// set via `sendString` functions and apply changes in `receiveString` functions.
        /// @param index index passed in `sendString` now received
        /// @param value value passed in `sendString` now received
        virtual void receiveString(int index, const char *value) {}

        /// Required to override for this effect's processing logic
        /// @param input  input buffer filled with data to process
        /// @param output output buffer to write to (comes cleared to 0)
        /// @param count  number of samples, the length of both input and output arrays. This value is guaranteed to
        ///               be a multiple of 4 for optimization purposes.
        /// @note both input and output buffers are interleaved stereo, so samples will occur in L-R-L-R order
        /// @returns whether anything has been processed. For efficiency if nothing should be altered, return false,
        ///          and it will act as if bypassed. Return true otherwise when data has been processed normally.
        virtual bool process(const float *input, float *output, int count) = 0;

        Engine *m_engine;
    };
}
