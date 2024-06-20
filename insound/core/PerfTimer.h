#pragma once

namespace insound {
    class PerfTimer {
    public:
        /// Starts the timer.
        static void start();

        /// Returns the number of nanoseconds that have passed since `start` was last called.
        static unsigned long long stop(bool log = false);
    };
}
