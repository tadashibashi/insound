#include <insound/core.h>
using namespace insound;

int main()
{
    auto effect = DelayEffect();
    effect.init(44100, .5, .5);

    float buffer[1024] = {0};
    PerfTimer::start();

    for (int i = 0; i < 100000; ++i)
        effect.process(buffer, buffer, 1024);

    const auto time = PerfTimer::stop();
    std::printf("Time: %llu ns\n", time);
}
