#include "Effect.h"
#include "Engine.h"
#include "Command.h"

namespace insound {

    void Effect::sendFloat(int index, float value)
    {
        m_engine->pushCommand(Command::makeEffectSetFloat(this, index, value));
    }

    void Effect::sendInt(int index, int value)
    {
        m_engine->pushCommand(Command::makeEffectSetInt(this, index, value));
    }

    void Effect::sendString(int index, const char *value)
    {
        m_engine->pushCommand(Command::makeEffectSetString(this, index, value));
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
