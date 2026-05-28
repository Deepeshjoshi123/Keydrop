#include <cassert>
#include <vector>

#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

int main()
{
    SchemaRuntime runtime;

    const SchemaDef schema {
        "StreamData",
        31,
        {
            FieldDef {"temperature", FieldType::u16, 0, {}},
            FieldDef {"device_id", FieldType::string, 1, FieldConstraints {true, 32}},
        }
    };
    assert(runtime.register_schema(schema).ok());

    RuntimeOptimizerConfig ro_cfg;
    ro_cfg.enabled = false;
    runtime.set_optimizer_config(ro_cfg);

    AdaptiveDictionaryConfig dict_cfg;
    dict_cfg.enabled = false;
    runtime.set_dictionary_config(dict_cfg);
    runtime.reset_dictionary();

    StreamOptimizerConfig stream_cfg;
    stream_cfg.enabled = true;
    stream_cfg.enable_packet_reuse = true;
    stream_cfg.enable_delta_updates = true;
    stream_cfg.enable_batching = true;
    stream_cfg.aggressive_after_samples = 1;
    stream_cfg.max_batch_packets = 3;
    stream_cfg.low_change_ratio_threshold = 0.5f;
    runtime.set_stream_optimizer_config(stream_cfg);
    runtime.reset_stream_optimizer();

    NamedPayload payload_a;
    payload_a["temperature"] = FieldValue::from_u16(100);
    payload_a["device_id"] = FieldValue::from_string("sensor_1");

    Buffer packet_first;
    bool has_packet = false;
    assert(runtime.send_stream("StreamData", payload_a, packet_first, has_packet).ok());
    assert(has_packet);

    Buffer packet_second;
    assert(runtime.send_stream("StreamData", payload_a, packet_second, has_packet).ok());
    assert(has_packet);
    assert(packet_second.size() == packet_first.size());
    assert(packet_second.data() == packet_first.data());

    NamedPayload payload_b = payload_a;
    payload_b["temperature"] = FieldValue::from_u16(101);

    Buffer stream_packet;
    assert(runtime.send_stream("StreamData", payload_b, stream_packet, has_packet).ok());
    assert(!has_packet);

    payload_b["temperature"] = FieldValue::from_u16(102);
    assert(runtime.send_stream("StreamData", payload_b, stream_packet, has_packet).ok());
    assert(!has_packet);

    payload_b["temperature"] = FieldValue::from_u16(103);
    assert(runtime.send_stream("StreamData", payload_b, stream_packet, has_packet).ok());
    assert(has_packet); // third queued item should emit batch

    std::vector<std::pair<std::string, NamedPayload>> decoded_messages;
    assert(runtime.receive_stream(stream_packet, decoded_messages).ok());
    assert(decoded_messages.size() == 3);
    assert(decoded_messages[0].second["temperature"].as_u16 == 101);
    assert(decoded_messages[1].second["temperature"].as_u16 == 102);
    assert(decoded_messages[2].second["temperature"].as_u16 == 103);

    // Sustained-load stability + decode correctness.
    usize sent = 0;
    usize received = 0;
    runtime.reset_stream_optimizer();
    for (usize i = 0; i < 2000; ++i)
    {
        NamedPayload p;
        p["temperature"] = FieldValue::from_u16(static_cast<u16>(200 + (i % 4)));
        p["device_id"] = FieldValue::from_string("sensor_1");

        Buffer out;
        bool has = false;
        assert(runtime.send_stream("StreamData", p, out, has).ok());
        sent += 1;
        if (has)
        {
            std::vector<std::pair<std::string, NamedPayload>> messages;
            assert(runtime.receive_stream(out, messages).ok());
            received += messages.size();
        }
    }

    Buffer flushed;
    bool flushed_has = false;
    assert(runtime.flush_stream(flushed, flushed_has).ok());
    if (flushed_has)
    {
        std::vector<std::pair<std::string, NamedPayload>> messages;
        assert(runtime.receive_stream(flushed, messages).ok());
        received += messages.size();
    }

    assert(received == sent);

    return 0;
}
