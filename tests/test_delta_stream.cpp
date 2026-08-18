#include <cassert>
#include <string>
#include <vector>

#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

namespace {

const SchemaDef kTelemetry {
    "Telemetry",
    55,
    {
        FieldDef {"timestamp", FieldType::u32, 0, {}},
        FieldDef {"device_id", FieldType::string, 1, FieldConstraints {true, 32}},
        FieldDef {"temperature", FieldType::u16, 2, {}},
        FieldDef {"status", FieldType::u8, 3, {}},
    }
};

SchemaRuntime make_runtime(usize keyframe_interval = 1000)
{
    SchemaRuntime runtime;
    assert(runtime.register_schema(kTelemetry).ok());

    StreamOptimizerConfig cfg;
    cfg.enabled = true;
    cfg.enable_packet_reuse = true;
    cfg.enable_delta_updates = true;
    cfg.enable_batching = false;
    cfg.enable_delta_packets = true;
    cfg.keyframe_interval = keyframe_interval;
    cfg.aggressive_after_samples = 1;
    cfg.low_change_ratio_threshold = 0.5f;
    runtime.set_stream_optimizer_config(cfg);
    runtime.reset_stream_optimizer();
    return runtime;
}

NamedPayload make_payload(u32 ts, u16 temp, u8 status)
{
    NamedPayload p;
    p["timestamp"] = FieldValue::from_u32(ts);
    p["device_id"] = FieldValue::from_string("sensor_01");
    p["temperature"] = FieldValue::from_u16(temp);
    p["status"] = FieldValue::from_u8(status);
    return p;
}

bool is_delta(const Buffer& packet)
{
    return !packet.empty() && packet.data()[0] == StreamOptimizer::kDeltaMarker;
}

} // namespace

int main()
{
    // ── First record is a full keyframe; later low-change records are deltas
    SchemaRuntime runtime = make_runtime();

    Buffer first;
    bool has_packet = false;
    assert(runtime.send_stream("Telemetry", make_payload(1000, 21, 1), first, has_packet).ok());
    assert(has_packet && !is_delta(first));

    Buffer second;
    assert(runtime.send_stream("Telemetry", make_payload(2000, 22, 1), second, has_packet).ok());
    assert(has_packet && is_delta(second));
    assert(second.size() < first.size());

    // ── Round-trip: delta expands and decodes to the exact values ──
    std::vector<std::pair<std::string, NamedPayload>> messages;
    assert(runtime.receive_stream(first, messages).ok() && messages.size() == 1);
    assert(runtime.receive_stream(second, messages).ok() && messages.size() == 1);
    assert(messages[0].first == "Telemetry");
    assert(messages[0].second["timestamp"].as_u32 == 2000);
    assert(messages[0].second["temperature"].as_u16 == 22);
    assert(messages[0].second["status"].as_u8 == 1);
    assert(messages[0].second["device_id"].as_string == "sensor_01");

    // ── Negative delta (signed sign-extension regression guard) ──
    Buffer negative;
    assert(runtime.send_stream("Telemetry", make_payload(3000, 20, 1), negative, has_packet).ok());
    assert(has_packet && is_delta(negative));
    assert(runtime.receive_stream(negative, messages).ok());
    assert(messages[0].second["temperature"].as_u16 == 20);
    assert(messages[0].second["timestamp"].as_u32 == 3000);

    // ── Unchanged record: all-unchanged delta, smaller than any full packet
    Buffer unchanged;
    assert(runtime.send_stream("Telemetry", make_payload(3000, 20, 1), unchanged, has_packet).ok());
    assert(has_packet && is_delta(unchanged));
    assert(unchanged.size() < second.size());
    assert(runtime.receive_stream(unchanged, messages).ok());

    // ── Keyframes: every interval-th record is a full packet; stream decodes
    SchemaRuntime keyed = make_runtime(5);
    usize emitted_full = 0;
    usize emitted_delta = 0;
    usize decoded = 0;
    std::vector<u32> expected_ts;
    for (usize i = 0; i < 25; ++i)
    {
        const u32 ts = 1000 + static_cast<u32>(i) * 1000;
        expected_ts.push_back(ts);
        Buffer out;
        bool has = false;
        assert(keyed.send_stream("Telemetry", make_payload(ts, static_cast<u16>(21 + i % 3), 1), out, has).ok());
        assert(has);
        if (is_delta(out))
        {
            emitted_delta += 1;
        }
        else
        {
            emitted_full += 1;
        }
        std::vector<std::pair<std::string, NamedPayload>> received;
        assert(keyed.receive_stream(out, received).ok());
        decoded += received.size();
        for (usize m = 0; m < received.size(); ++m)
        {
            assert(received[m].second["timestamp"].as_u32 == expected_ts[decoded - received.size() + m]);
        }
    }
    assert(decoded == 25);
    assert(emitted_full >= 5); // initial + every 5th record
    assert(emitted_delta > 0);

    // ── Loss safety: dropped delta → next delta rejected, keyframe recovers
    SchemaRuntime sender = make_runtime();
    SchemaRuntime receiver;
    assert(receiver.register_schema(kTelemetry).ok());
    StreamOptimizerConfig receiver_cfg;
    receiver_cfg.enabled = true;
    receiver_cfg.enable_delta_packets = true;
    receiver_cfg.aggressive_after_samples = 1;
    receiver.set_stream_optimizer_config(receiver_cfg);

    std::vector<Buffer> emitted;
    for (usize i = 0; i < 8; ++i)
    {
        Buffer out;
        bool has = false;
        assert(sender.send_stream("Telemetry", make_payload(1000 + static_cast<u32>(i) * 1000, static_cast<u16>(30 + i), 1), out, has).ok());
        if (has)
        {
            emitted.push_back(out);
        }
    }
    for (usize i = 0; i < emitted.size(); ++i)
    {
        std::vector<std::pair<std::string, NamedPayload>> received;
        if (i == 4)
        {
            continue; // simulate dropped packet
        }
        const SchemaRuntimeResult result = receiver.receive_stream(emitted[i], received);
        if (i < 4)
        {
            assert(result.ok()); // state in sync before the drop
        }
        else if (i == 5)
        {
            assert(result.code == SchemaRuntimeCode::decode_failed); // safe rejection, never misdecode
        }
    }

    // Sender reset → next emission is a keyframe → receiver resynchronizes.
    sender.reset_stream_optimizer();
    Buffer keyframe;
    bool has = false;
    assert(sender.send_stream("Telemetry", make_payload(9000, 38, 1), keyframe, has).ok());
    assert(has && !is_delta(keyframe));
    std::vector<std::pair<std::string, NamedPayload>> recovered;
    assert(receiver.receive_stream(keyframe, recovered).ok());
    assert(recovered.size() == 1 && recovered[0].second["temperature"].as_u16 == 38);

    Buffer delta_after_recovery;
    assert(sender.send_stream("Telemetry", make_payload(10000, 39, 1), delta_after_recovery, has).ok());
    assert(has && is_delta(delta_after_recovery));
    assert(receiver.receive_stream(delta_after_recovery, recovered).ok());
    assert(recovered[0].second["temperature"].as_u16 == 39);

    // ── Dictionary reset control packet (3A) ─────────────────────
    SchemaRuntime dict_sender;
    const SchemaDef dict_schema {
        "DictStream",
        56,
        {
            FieldDef {"id", FieldType::string, 0, FieldConstraints {true, 64}},
            FieldDef {"v", FieldType::u16, 1, {}},
        }
    };
    assert(dict_sender.register_schema(dict_schema).ok());
    AdaptiveDictionaryConfig dict_config;
    dict_config.enabled = true;
    dict_config.enable_string_values = true;
    dict_sender.set_dictionary_config(dict_config);

    NamedPayload dp;
    dp["id"] = FieldValue::from_string("device-42");
    dp["v"] = FieldValue::from_u16(1);
    Buffer d1;
    Buffer d2;
    assert(dict_sender.send("DictStream", dp, d1).ok());
    assert(dict_sender.send("DictStream", dp, d2).ok());
    assert(d2.size() < d1.size()); // dictionary reference in use

    Buffer control;
    assert(dict_sender.send_dictionary_reset(control).ok());
    std::vector<std::pair<std::string, NamedPayload>> control_messages;
    assert(dict_sender.receive_stream(control, control_messages).ok());
    assert(control_messages.empty());

    // The stale reference packet is now safely rejected, never misdecoded.
    std::string stale_schema;
    NamedPayload stale_payload;
    const SchemaRuntimeResult stale = dict_sender.receive(d2, stale_schema, stale_payload);
    assert(stale.code == SchemaRuntimeCode::decode_failed);

    // After reset both sides restart from full strings.
    dict_sender.reset_dictionary();
    NamedPayload fp;
    fp["id"] = FieldValue::from_string("device-43");
    fp["v"] = FieldValue::from_u16(2);
    Buffer full_after_reset;
    assert(dict_sender.send("DictStream", fp, full_after_reset).ok());
    std::string decoded_schema;
    NamedPayload decoded_payload;
    assert(dict_sender.receive(full_after_reset, decoded_schema, decoded_payload).ok());
    assert(decoded_payload["id"].as_string == "device-43");

    // ── u32 delta overflow → raw fallback still round-trips ───────
    SchemaRuntime edge = make_runtime();
    Buffer big_first;
    assert(edge.send_stream("Telemetry", make_payload(4294967295u, 21, 1), big_first, has).ok());
    assert(has && !is_delta(big_first));
    assert(edge.receive_stream(big_first, messages).ok());

    Buffer big_delta;
    assert(edge.send_stream("Telemetry", make_payload(0, 21, 1), big_delta, has).ok());
    assert(has && is_delta(big_delta));
    assert(edge.receive_stream(big_delta, messages).ok());
    assert(messages[0].second["timestamp"].as_u32 == 0);

    // ── Delta disabled when overhead is not repaid ────────────────
    SchemaRuntime tiny_runtime;
    const SchemaDef tiny_schema {
        "Tiny",
        57,
        {
            FieldDef {"x", FieldType::u8, 0, {}},
        }
    };
    assert(tiny_runtime.register_schema(tiny_schema).ok());
    StreamOptimizerConfig tiny_cfg;
    tiny_cfg.enabled = true;
    tiny_cfg.enable_delta_packets = true;
    tiny_cfg.aggressive_after_samples = 1;
    tiny_cfg.low_change_ratio_threshold = 1.0f; // always attempt
    tiny_runtime.set_stream_optimizer_config(tiny_cfg);

    NamedPayload t1;
    t1["x"] = FieldValue::from_u8(1);
    Buffer tiny_first;
    assert(tiny_runtime.send_stream("Tiny", t1, tiny_first, has).ok());
    assert(has && !is_delta(tiny_first)); // keyframe

    NamedPayload t2;
    t2["x"] = FieldValue::from_u8(2);
    Buffer tiny_second;
    assert(tiny_runtime.send_stream("Tiny", t2, tiny_second, has).ok());
    assert(has && !is_delta(tiny_second)); // 8-byte delta >= 3-byte full → full emitted
    assert(tiny_second.size() == 3);
    assert(tiny_runtime.receive_stream(tiny_second, messages).ok());

    return 0;
}
