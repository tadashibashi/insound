#include "PerfTimer.h"
#include <chrono>

using Clock = std::chrono::high_resolution_clock;
using Nano = std::chrono::nanoseconds;

template<typename Duration>
using TimePoint = std::chrono::time_point<Clock, Duration>;

static Clock::time_point s_startTime;

void insound::PerfTimer::start()
{
    s_startTime = Clock::now();
}

unsigned long long insound::PerfTimer::stop(const bool log)
{
    const auto duration = Clock::now() - s_startTime;
    const auto ns = std::chrono::duration_cast<Nano>( duration ).count();

    if (log)
    {
        std::printf("Time in ns: %llu\n", ns);
    }

    return ns;
}
