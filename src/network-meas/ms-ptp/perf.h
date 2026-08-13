#ifndef _PERF_H_
#define _PERF_H_

#include <atomic>
#include <chrono>

class Profiler
{
private:
    std::atomic_uint64_t duration;

public:
    Profiler() : duration(0) {}
    void mark_start();
    void mark_end();
    uint64_t reset_and_output();
};

#endif