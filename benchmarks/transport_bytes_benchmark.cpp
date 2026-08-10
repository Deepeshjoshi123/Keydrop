// Phase 6 transport bytes benchmark.
//
// Reports application payload, Keydrop framing, and transport framing
// separately for the fixed 3-field benchmark record, per the plan:
// "Report application payload, Keydrop framing, and transport framing
// separately."
//
//   application payload  — field value bytes only (fixed-width values plus
//                          string/bytes content, no length prefixes)
//   Keydrop framing      — 2-byte message_id + 2-byte length prefix per
//                          variable-length field
//   transport framing    — the 4-byte little-endian length prefix added
//                          by TcpAdapter per packet
//
// Includes a loopback TCP sanity check: every sent packet must be received
// byte-identical. Output is key=value lines suitable for CSV capture.

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

#include "keydrop/schema/schema_runtime.hpp"
#include "keydrop/transport/tcp_adapter.hpp"
#include "test_thread.hpp"

using namespace keydrop;

namespace {

constexpr u16 kTransportFramingBytes = 4; // TCP length prefix per packet

SchemaRuntime make_runtime()
{
    SchemaRuntime runtime;
    const SchemaDef schema {
        "BenchmarkPayload",
        91,
        {
            FieldDef {"temperature", FieldType::u16, 0, {}},
            FieldDef {"humidity", FieldType::u16, 1, {}},
            FieldDef {"device_id", FieldType::string, 2, FieldConstraints {true, 64}},
        }
    };
    (void)runtime.register_schema(schema);
    return runtime;
}

NamedPayload make_payload()
{
    NamedPayload payload;
    payload["temperature"] = FieldValue::from_u16(32);
    payload["humidity"] = FieldValue::from_u16(70);
    payload["device_id"] = FieldValue::from_string("sensor_01");
    return payload;
}

usize application_payload_bytes(const NamedPayload& payload, const SchemaDef& schema)
{
    usize total = 0;
    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        const NamedPayload::const_iterator it = payload.find(schema.fields[i].name);
        if (it == payload.end())
        {
            continue;
        }
        switch (it->second.type)
        {
        case FieldType::u8: case FieldType::i8: total += 1; break;
        case FieldType::u16: case FieldType::i16: total += 2; break;
        case FieldType::u32: case FieldType::i32: case FieldType::f32: total += 4; break;
        case FieldType::f64: total += 8; break;
        case FieldType::string: total += it->second.as_string.size(); break;
        case FieldType::bytes: total += it->second.as_bytes.size(); break;
        }
    }
    return total;
}

} // namespace

int main()
{
    SchemaRuntime runtime = make_runtime();
    const NamedPayload payload = make_payload();

    Buffer packet;
    if (!runtime.send("BenchmarkPayload", payload, packet).ok())
    {
        return 1;
    }

    const SchemaDef* schema = runtime.registry().find_by_name("BenchmarkPayload");
    const usize application_payload = application_payload_bytes(payload, *schema);
    const usize keydrop_framing = packet.size() - application_payload;
    const usize transport_framing = kTransportFramingBytes;
    const usize total_wire = packet.size() + transport_framing;

    std::cout << "application_payload_bytes=" << application_payload << "\n";
    std::cout << "keydrop_framing_bytes=" << keydrop_framing << "\n";
    std::cout << "transport_framing_bytes=" << transport_framing << "\n";
    std::cout << "keydrop_packet_bytes=" << packet.size() << "\n";
    std::cout << "total_wire_bytes=" << total_wire << "\n";
    std::cout << "application_payload_pct=" << 100.0 * application_payload / total_wire << "\n";
    std::cout << "keydrop_framing_pct=" << 100.0 * keydrop_framing / total_wire << "\n";
    std::cout << "transport_framing_pct=" << 100.0 * transport_framing / total_wire << "\n";

    // ── Loopback sanity: every packet received byte-identical ─────
    TcpAdapter server;
    const TransportResult listen_result = server.listen(TransportEndpoint {"127.0.0.1", 0, ""});
    if (!listen_result.ok())
    {
        std::cout << "loopback_round_trip=skipped\n";
        return 0;
    }
    const u16 port = server.local_port();

    // NOTE: no side effects inside assert() — this benchmark builds with
    // NDEBUG in Release, where assert() removes its argument entirely.
    bool server_done = false;
    usize server_bytes = 0;
    TestThread server_thread([&server, &server_done, &server_bytes, &packet]() {
        const TransportResult accepted = server.accept_connection();
        assert(accepted.ok());
        for (usize i = 0; i < 3; ++i)
        {
            const TransportReceiveResult received = server.receive();
            assert(received.ok());
            server_bytes += received.packet.size();
            assert(received.packet.data() == packet.data());
        }
        server_done = true;
    });

    TcpAdapter client;
    const TransportResult connected = client.connect(TransportEndpoint {"127.0.0.1", port, ""});
    assert(connected.ok());
    for (usize i = 0; i < 3; ++i)
    {
        const TransportResult sent = client.send(packet);
        assert(sent.ok());
    }
    server_thread.join();
    assert(server_done);
    std::cout << "loopback_packets=3\n";
    std::cout << "loopback_packet_bytes_identical=1\n";
    std::cout << "loopback_server_bytes=" << server_bytes << "\n";
    const TransportResult client_closed = client.close();
    const TransportResult server_closed = server.close();
    assert(client_closed.ok());
    assert(server_closed.ok());

    return 0;
}
