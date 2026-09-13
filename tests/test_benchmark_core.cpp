#include <cassert>

#include "keydrop/benchmark/benchmark_core.hpp"

using namespace keydrop;

int main()
{
    Buffer packet;
    packet.write(0xAA);
    packet.write(0xBB);
    packet.write(0xCC);
    assert(measure_packet_size(packet) == 3);

    assert(average_latency_ns(1000, 10) == 100.0);
    assert(average_latency_ns(1000, 0) == 0.0);

    assert(throughput_per_second(100, 1000000000) == 100.0);
    assert(throughput_per_second(100, 0) == 0.0);
    assert(throughput_per_second(0, 1000000000) == 0.0);

    AllocationTracker tracker;
    assert(tracker.allocations() == 0);
    assert(tracker.allocated_bytes() == 0);
    tracker.record(16);
    tracker.record(32);
    assert(tracker.allocations() == 2);
    assert(tracker.allocated_bytes() == 48);
    tracker.reset();
    assert(tracker.allocations() == 0);
    assert(tracker.allocated_bytes() == 0);

    BenchmarkSample sample;
    sample.name = "keydrop";
    sample.iterations = 100;
    sample.packet_size_bytes = 12;
    sample.encode_time_ns = 2000;
    sample.decode_time_ns = 3000;
    sample.allocations = 4;
    sample.allocated_bytes = 128;

    const BenchmarkResult result = summarize_sample(sample);
    assert(result.name == "keydrop");
    assert(result.iterations == 100);
    assert(result.packet_size_bytes == 12);
    assert(result.encode_latency_ns == 20.0);
    assert(result.decode_latency_ns == 30.0);
    assert(result.throughput_per_sec == 20000000.0);
    assert(result.allocations == 4);
    assert(result.allocated_bytes == 128);

    BenchmarkTimer timer;
    const u64 elapsed = timer.elapsed_ns();
    assert(elapsed >= 0);
    timer.reset();
    assert(timer.elapsed_ns() >= 0);

    return 0;
}
