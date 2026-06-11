#include <cassert>
#include <vector>

#include "keydrop/benchmark/format_benchmark.hpp"

using namespace keydrop;

int main()
{
    const BenchmarkPayload payload = default_benchmark_payload();

    const EncodedPayload keydrop = encode_keydrop_payload(payload);
    const EncodedPayload json = encode_json_payload(payload);
    const EncodedPayload protobuf = encode_protobuf_payload(payload);
    const EncodedPayload messagepack = encode_messagepack_payload(payload);

    assert(keydrop.format_name == "keydrop");
    assert(json.format_name == "json");
    assert(protobuf.format_name == "protobuf");
    assert(messagepack.format_name == "messagepack");

    assert(keydrop.bytes.size() > 0);
    assert(json.bytes.size() > 0);
    assert(protobuf.bytes.size() > 0);
    assert(messagepack.bytes.size() > 0);

    BenchmarkPayload decoded_keydrop;
    BenchmarkPayload decoded_json;
    BenchmarkPayload decoded_protobuf;
    BenchmarkPayload decoded_messagepack;

    assert(decode_keydrop_payload(keydrop.bytes, decoded_keydrop));
    assert(decode_json_payload(json.bytes, decoded_json));
    assert(decode_protobuf_payload(protobuf.bytes, decoded_protobuf));
    assert(decode_messagepack_payload(messagepack.bytes, decoded_messagepack));

    assert(payloads_equal(payload, decoded_keydrop));
    assert(payloads_equal(payload, decoded_json));
    assert(payloads_equal(payload, decoded_protobuf));
    assert(payloads_equal(payload, decoded_messagepack));

    Buffer bad_packet;
    bad_packet.write(0xFF);
    assert(!decode_keydrop_payload(bad_packet, decoded_keydrop));
    assert(!decode_json_payload(bad_packet, decoded_json));
    assert(!decode_protobuf_payload(bad_packet, decoded_protobuf));
    assert(!decode_messagepack_payload(bad_packet, decoded_messagepack));

    const usize iterations = 8;
    const BenchmarkSample keydrop_sample = run_keydrop_benchmark(iterations);
    const BenchmarkSample json_sample = run_json_benchmark(iterations);
    const BenchmarkSample protobuf_sample = run_protobuf_benchmark(iterations);
    const BenchmarkSample messagepack_sample = run_messagepack_benchmark(iterations);

    assert(keydrop_sample.iterations == iterations);
    assert(json_sample.iterations == iterations);
    assert(protobuf_sample.iterations == iterations);
    assert(messagepack_sample.iterations == iterations);

    assert(keydrop_sample.packet_size_bytes == keydrop.bytes.size());
    assert(json_sample.packet_size_bytes == json.bytes.size());
    assert(protobuf_sample.packet_size_bytes == protobuf.bytes.size());
    assert(messagepack_sample.packet_size_bytes == messagepack.bytes.size());

    assert(keydrop_sample.allocations == iterations);
    assert(json_sample.allocations == iterations);
    assert(protobuf_sample.allocations == iterations);
    assert(messagepack_sample.allocations == iterations);

    const std::vector<BenchmarkSample> samples = run_format_benchmarks(iterations);
    assert(samples.size() == 4);
    assert(samples[0].name == "keydrop");
    assert(samples[1].name == "json");
    assert(samples[2].name == "protobuf");
    assert(samples[3].name == "messagepack");

    return 0;
}
