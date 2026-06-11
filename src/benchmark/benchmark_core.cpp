#include "keydrop/benchmark/benchmark_core.hpp"

namespace keydrop {

BenchmarkTimer::BenchmarkTimer()
    : start_(clock::now())
{
}

void BenchmarkTimer::reset()
{
    start_ = clock::now();
}

u64 BenchmarkTimer::elapsed_ns() const
{
    const clock::time_point now = clock::now();
    return static_cast<u64>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_).count()
    );
}

void AllocationTracker::reset()
{
    allocations_ = 0;
    allocated_bytes_ = 0;
}

void AllocationTracker::record(usize bytes)
{
    allocations_ += 1;
    allocated_bytes_ += bytes;
}

usize AllocationTracker::allocations() const
{
    return allocations_;
}

usize AllocationTracker::allocated_bytes() const
{
    return allocated_bytes_;
}

usize measure_packet_size(const Buffer& packet)
{
    return packet.size();
}

double average_latency_ns(u64 total_time_ns, usize iterations)
{
    if (iterations == 0)
    {
        return 0.0;
    }

    return static_cast<double>(total_time_ns) / static_cast<double>(iterations);
}

double throughput_per_second(usize operations, u64 total_time_ns)
{
    if (operations == 0 || total_time_ns == 0)
    {
        return 0.0;
    }

    const double seconds = static_cast<double>(total_time_ns) / 1000000000.0;
    return static_cast<double>(operations) / seconds;
}

BenchmarkResult summarize_sample(const BenchmarkSample& sample)
{
    BenchmarkResult result;
    result.name = sample.name;
    result.iterations = sample.iterations;
    result.packet_size_bytes = sample.packet_size_bytes;
    result.encode_latency_ns = average_latency_ns(
        sample.encode_time_ns,
        sample.iterations
    );
    result.decode_latency_ns = average_latency_ns(
        sample.decode_time_ns,
        sample.iterations
    );
    result.throughput_per_sec = throughput_per_second(
        sample.iterations,
        sample.encode_time_ns + sample.decode_time_ns
    );
    result.allocations = sample.allocations;
    result.allocated_bytes = sample.allocated_bytes;
    return result;
}

}
