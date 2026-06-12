#pragma once

#include <string>
#include <vector>

#include "keydrop/benchmark/benchmark_core.hpp"

namespace keydrop {

std::string format_benchmark_table(const std::vector<BenchmarkResult>& results);
bool save_benchmark_report(const std::string& path, const std::string& content);

}
