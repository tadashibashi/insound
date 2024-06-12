#pragma once

namespace insound {
    struct EffectCommand;
    class Engine;

    class Effect {
    public:
        virtual ~Effect() = default;
    protected:
        Effect() : m_engine() { }
        void sendFloat(int index, float value);
        void sendInt(int index, int value);
        void sendString(int index, const char *value);

        [[nodiscard]]
        Engine *engine() { return m_engine; }
        [[nodiscard]]
        const Engine *engine() const { return m_engine; }
    private:
        friend class Engine;
        friend class Source;
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
        virtual void process(float *input, float *output, int count) = 0;

        Engine *m_engine;
    };
}
