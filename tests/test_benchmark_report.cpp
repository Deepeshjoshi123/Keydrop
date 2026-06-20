#include <cassert>
#include <fstream>
#include <string>
#include <vector>

#include "keydrop/benchmark/benchmark_report.hpp"

using namespace keydrop;

int main()
{
    BenchmarkResult keydrop;
    keydrop.name = "keydrop";
    keydrop.iterations = 10;
    keydrop.packet_size_bytes = 17;
    keydrop.encode_latency_ns = 12.5;
    keydrop.decode_latency_ns = 14.25;
    keydrop.allocations = 10;
    keydrop.allocated_bytes = 170;
    keydrop.throughput_per_sec = 1000.0;

    BenchmarkResult json = keydrop;
    json.name = "json";
    json.packet_size_bytes = 58;

    const std::vector<BenchmarkResult> results = {keydrop, json};
    const std::string report = format_benchmark_table(results);
    assert(report.find("Keydrop Benchmark Results") != std::string::npos);
    assert(report.find("format") != std::string::npos);
    assert(report.find("packet") != std::string::npos);
    assert(report.find("encode_ns") != std::string::npos);
    assert(report.find("decode_ns") != std::string::npos);
    assert(report.find("allocs") != std::string::npos);
    assert(report.find("throughput/s") != std::string::npos);
    assert(report.find("keydrop") != std::string::npos);
    assert(report.find("json") != std::string::npos);

    const std::string path = "keydrop_benchmark_report_test.txt";
    assert(save_benchmark_report(path, report));

    std::ifstream saved(path.c_str());
    assert(saved.good());
    std::string saved_content;
    std::getline(saved, saved_content);
    assert(saved_content == "Keydrop Benchmark Results");

    assert(!save_benchmark_report("", report));

    return 0;
}
