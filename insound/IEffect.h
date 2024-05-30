#pragma once

namespace insound {
    class Engine;

    class IEffect {
    public:
        explicit IEffect(Engine *engine) : m_engine(engine) { }
        virtual ~IEffect() = default;

        virtual void process(float *input, float *output, int count) = 0;

        [[nodiscard]]
        Engine *engine() { return m_engine; }
        [[nodiscard]]
        const Engine *engine() const { return m_engine; }
    protected:
        void sendParam(int index, float value);

        /// Override this if you need to process parameter sets.
        /// Don't set member parameters directly as it may occur during audio thread.
        /// Instead set via `setParam` and apply changes in `receiveParam`.
        virtual void receiveParam(int index, float value) {}

    private:
        friend class Engine;
        Engine *m_engine;
    };
}
