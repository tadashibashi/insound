#include "IEffect.h"
#include "Engine.h"
#include "Command.h"
namespace insound {
    void IEffect::sendParam(int index, float value)
    {
        m_engine->pushCommand(Command::makeEffectParamSet(this, index, value));
    }
} // insound