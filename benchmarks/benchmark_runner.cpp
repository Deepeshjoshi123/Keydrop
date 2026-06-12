#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "keydrop/benchmark/benchmark_report.hpp"
#include "keydrop/benchmark/format_benchmark.hpp"

using namespace keydrop;

namespace {

usize parse_iterations(int argc, char** argv)
{
    if (argc < 2)
    {
        return 1000;
    }

    const long parsed = std::strtol(argv[1], nullptr, 10);
    if (parsed <= 0)
    {
        return 1000;
    }

    return static_cast<usize>(parsed);
}

std::string parse_output_path(int argc, char** argv)
{
    if (argc < 3)
    {
        return "benchmark_report.txt";
    }

    return argv[2];
}

} // namespace

int main(int argc, char** argv)
{
    const usize iterations = parse_iterations(argc, argv);
    const std::string output_path = parse_output_path(argc, argv);

    const std::vector<BenchmarkSample> samples =
        run_format_benchmarks(iterations);

    std::vector<BenchmarkResult> results;
    results.reserve(samples.size());
    for (usize i = 0; i < samples.size(); ++i)
    {
        results.push_back(summarize_sample(samples[i]));
    }

    const std::string report = format_benchmark_table(results);
    std::cout << report;

    if (!save_benchmark_report(output_path, report))
    {
        std::cerr << "failed to save benchmark report: " << output_path << "\n";
        return 1;
    }

    std::cout << "saved_report=" << output_path << "\n";
    return 0;
}
