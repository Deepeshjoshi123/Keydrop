#include "keydrop/benchmark/benchmark_report.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace keydrop {

std::string format_benchmark_table(const std::vector<BenchmarkResult>& results)
{
    std::ostringstream out;
    out << "Keydrop Benchmark Results\n";
    out << "=========================\n";
    out << std::left
        << std::setw(14) << "format"
        << std::right
        << std::setw(12) << "packet"
        << std::setw(16) << "encode_ns"
        << std::setw(16) << "decode_ns"
        << std::setw(16) << "allocs"
        << std::setw(18) << "alloc_bytes"
        << std::setw(18) << "throughput/s"
        << "\n";

    out << std::string(110, '-') << "\n";

    out << std::fixed << std::setprecision(2);
    for (usize i = 0; i < results.size(); ++i)
    {
        const BenchmarkResult& result = results[i];
        out << std::left
            << std::setw(14) << result.name
            << std::right
            << std::setw(12) << result.packet_size_bytes
            << std::setw(16) << result.encode_latency_ns
            << std::setw(16) << result.decode_latency_ns
            << std::setw(16) << result.allocations
            << std::setw(18) << result.allocated_bytes
            << std::setw(18) << result.throughput_per_sec
            << "\n";
    }

    return out.str();
}

bool save_benchmark_report(const std::string& path, const std::string& content)
{
    std::ofstream file(path.c_str(), std::ios::out | std::ios::trunc);
    if (!file)
    {
        return false;
    }

    file << content;
    return file.good();
}

}
