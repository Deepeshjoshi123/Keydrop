#pragma once

#include <chrono>
#include <string>

#include "keydrop/core/buffer.hpp"
#include "keydrop/core/types.hpp"

namespace keydrop {

struct BenchmarkSample {
    std::string name;
    usize iterations = 0;
    usize packet_size_bytes = 0;
    u64 encode_time_ns = 0;
    u64 decode_time_ns = 0;
    usize allocations = 0;
    usize allocated_bytes = 0;
};

struct BenchmarkResult {
    std::string name;
    usize iterations = 0;
    usize packet_size_bytes = 0;
    double encode_latency_ns = 0.0;
    double decode_latency_ns = 0.0;
    double throughput_per_sec = 0.0;
    usize allocations = 0;
    usize allocated_bytes = 0;
};

class BenchmarkTimer {
public:
    using clock = std::chrono::steady_clock;

    BenchmarkTimer();

    void reset();
    u64 elapsed_ns() const;

private:
    clock::time_point start_;
};

class AllocationTracker {
public:
    void reset();
    void record(usize bytes);

    usize allocations() const;
    usize allocated_bytes() const;

private:
    usize allocations_ = 0;
    usize allocated_bytes_ = 0;
};

usize measure_packet_size(const Buffer& packet);
double average_latency_ns(u64 total_time_ns, usize iterations);
double throughput_per_second(usize operations, u64 total_time_ns);
BenchmarkResult summarize_sample(const BenchmarkSample& sample);

}
