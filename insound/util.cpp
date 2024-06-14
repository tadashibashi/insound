#include "util.h"

float insound::clamp(float value, float min, float max)
{
    return value < min ? min :
        value > max ? max :
        value;
}

int insound::clampi(int value, int min, int max)
{
    return value < min ? min :
    value > max ? max :
    value;
}

uint32_t insound::align(uint32_t alignment, uint32_t value)
{
    return (value + alignment - 1) & ~(alignment - 1);
}
