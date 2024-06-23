#include "PerfTimer.h"
#include "logging.h"

#include <chrono>
#include <cstdio>

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
#ifdef __EMSCRIPTEN__ // Web environment only supports accuracy in milliseconds
        INSOUND_LOG("Time in ms: %llu\n", static_cast<uint64_t>(ns * 0.000001));
#else
        INSOUND_LOG("Time in ns: %llu\n", ns);
#endif
    }

    return ns;
}
