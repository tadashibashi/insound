#include "Effect.h"
#include "Engine.h"
#include "Command.h"
namespace insound {
    void Effect::sendParam(int index, float value)
    {
        m_engine->pushCommand(Command::makeEffectParamSet(this, index, value));
    }
} // insound