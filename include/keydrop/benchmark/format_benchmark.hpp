#pragma once

#include <string>
#include <vector>

#include "keydrop/benchmark/benchmark_core.hpp"
#include "keydrop/core/buffer.hpp"

namespace keydrop {

struct BenchmarkPayload {
    u16 temperature = 32;
    u16 humidity = 70;
    std::string device_id = "sensor_01";
};

struct EncodedPayload {
    std::string format_name;
    Buffer bytes;
};

BenchmarkPayload default_benchmark_payload();
bool payloads_equal(const BenchmarkPayload& left, const BenchmarkPayload& right);

EncodedPayload encode_keydrop_payload(const BenchmarkPayload& payload);
EncodedPayload encode_json_payload(const BenchmarkPayload& payload);
EncodedPayload encode_protobuf_payload(const BenchmarkPayload& payload);
EncodedPayload encode_messagepack_payload(const BenchmarkPayload& payload);

bool decode_keydrop_payload(const Buffer& bytes, BenchmarkPayload& out_payload);
bool decode_json_payload(const Buffer& bytes, BenchmarkPayload& out_payload);
bool decode_protobuf_payload(const Buffer& bytes, BenchmarkPayload& out_payload);
bool decode_messagepack_payload(const Buffer& bytes, BenchmarkPayload& out_payload);

BenchmarkSample run_keydrop_benchmark(usize iterations);
BenchmarkSample run_json_benchmark(usize iterations);
BenchmarkSample run_protobuf_benchmark(usize iterations);
BenchmarkSample run_messagepack_benchmark(usize iterations);

std::vector<BenchmarkSample> run_format_benchmarks(usize iterations);

}
