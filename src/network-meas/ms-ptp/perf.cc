#include "perf.h"


void Profiler::mark_start()
{
    duration -= std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

void Profiler::mark_end()
{
    duration += std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

uint64_t Profiler::reset_and_output()
{
    uint64_t val = duration.exchange(0);
    return val;
}

