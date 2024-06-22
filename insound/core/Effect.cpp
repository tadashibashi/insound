#include "Effect.h"
#include "Engine.h"
#include "Command.h"

namespace insound {
#define HANDLE_GUARD() do { if (detail::peekSystemError().code == Result::InvalidHandle) { \
    detail::popSystemError(); \
    pushError(Result::InvalidHandle, __FUNCTION__); \
    return false; \
} } while(0)

    Effect::Effect(Effect &&other) noexcept : m_engine(other.m_engine)
    {}

    bool Effect::sendFloat(int index, float value)
    {
        HANDLE_GUARD();

        return m_engine->pushCommand(Command::makeEffectSetFloat(this, index, value));
    }

    bool Effect::sendInt(int index, int value)
    {
        HANDLE_GUARD();

        return m_engine->pushCommand(Command::makeEffectSetInt(this, index, value));
    }

    bool Effect::sendString(int index, const char *value)
    {
        HANDLE_GUARD();

        return m_engine->pushCommand(Command::makeEffectSetString(this, index, value));
    }

    void Effect::applyCommand(const EffectCommand &command)
    {
        switch(command.type)
        {
            case EffectCommand::SetFloat:
            {
                receiveFloat(command.setfloat.index, command.setfloat.value);
            } break;

            case EffectCommand::SetInt:
            {
                receiveInt(command.setint.index, command.setint.value);
            } break;

            case EffectCommand::SetString:
            {
                receiveString(command.setstring.index, command.setstring.value);
            } break;

            default:
            {

            } break;
        }
    }
}
