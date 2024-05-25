#include "mathf.h"

float insound::mathf::clamp(float value, float min, float max)
{
    return value < min ? min :
        value > max ? max :
        value;
}

int insound::mathf::clampi(int value, int min, int max)
{
    return value < min ? min :
    value > max ? max :
    value;
}
