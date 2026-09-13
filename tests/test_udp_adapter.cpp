// Phase 6 UDP tests: round-trip, MTU bound, sequence/loss/reordering
// detection, and Keydrop-level recovery over corrupted datagram payloads.

#include <cassert>
#include <string>
#include <vector>

#include "keydrop/schema/schema_runtime.hpp"
#include "keydrop/transport/udp_adapter.hpp"

using namespace keydrop;

namespace {

Buffer make_packet(std::initializer_list<byte> bytes)
{
    Buffer packet;
    for (byte value : bytes)
    {
        packet.write(value);
    }
    return packet;
}

bool blocked(const TransportResult& result)
{
    return !result.ok()
        && (result.code == TransportStatusCode::listen_failed
            || result.code == TransportStatusCode::bind_failed)
        && result.message.find("Operation not permitted") != std::string::npos;
}

} // namespace

int main()
{
    // ── Round-trip over loopback ─────────────────────────────────
    {
        UdpAdapter server;
        const TransportResult bound = server.listen(TransportEndpoint {"127.0.0.1", 0, ""});
        if (blocked(bound))
        {
            return 77;
        }
        assert(bound.ok());
        const u16 port = server.local_port();
        assert(port != 0);

        UdpAdapter client;
        assert(client.connect(TransportEndpoint {"127.0.0.1", port, ""}).ok());

        const Buffer sent = make_packet({0xAA, 0xBB, 0xCC});
        assert(client.send(sent).ok());
        const TransportReceiveResult received = server.receive();
        assert(received.ok());
        assert(received.packet.data() == sent.data());

        // Reply path.
        const Buffer reply = make_packet({0x11});
        assert(server.send(reply).ok());
        const TransportReceiveResult got_reply = client.receive();
        assert(got_reply.ok());
        assert(got_reply.packet.data() == reply.data());

        assert(client.close().ok());
        assert(server.close().ok());
    }

    // ── MTU bound: oversized packets are rejected ────────────────
    {
        UdpAdapter server;
        const TransportResult bound = server.listen(TransportEndpoint {"127.0.0.1", 0, ""});
        if (blocked(bound))
        {
            return 77;
        }
        assert(bound.ok());

        UdpConfig small_config;
        small_config.max_datagram_bytes = 64;
        UdpAdapter client(small_config);
        assert(client.connect(TransportEndpoint {"127.0.0.1", server.local_port(), ""}).ok());

        Buffer tiny = make_packet({0x01});
        assert(client.send(tiny).ok());

        Buffer oversized;
        for (usize i = 0; i < 100; ++i)
        {
            oversized.write(static_cast<byte>(i));
        }
        const TransportResult rejected = client.send(oversized);
        assert(!rejected.ok() && rejected.code == TransportStatusCode::send_failed);

        assert(client.close().ok());
        assert(server.close().ok());
    }

    // ── Sequence: loss and reordering are detected, not guessed ──
    {
        UdpAdapter server;
        const TransportResult bound = server.listen(TransportEndpoint {"127.0.0.1", 0, ""});
        if (blocked(bound))
        {
            return 77;
        }
        assert(bound.ok());

        UdpConfig client_config;
        client_config.drop_sequence = 3; // datagram 3 is lost on the wire
        UdpAdapter client(client_config);
        assert(client.connect(TransportEndpoint {"127.0.0.1", server.local_port(), ""}).ok());

        // Datagrams 1, 2, 4 arrive (3 is "lost" on the wire): one gap.
        assert(client.send(make_packet({0x01})).ok());
        assert(server.receive().ok());
        assert(client.send(make_packet({0x02})).ok());
        assert(server.receive().ok());
        assert(client.send(make_packet({0x03})).ok()); // dropped by the hook
        assert(client.send(make_packet({0x04})).ok());
        assert(server.receive().ok());
        assert(server.stats().datagrams_received == 3);
        assert(server.stats().lost == 1); // seq 3 never arrived
        assert(server.stats().reordered == 0);

        assert(client.close().ok());
        assert(server.close().ok());
    }

    // ── Reordering: a fresh sender's seq 1 arriving after seq 3 ──
    {
        UdpAdapter server;
        const TransportResult bound = server.listen(TransportEndpoint {"127.0.0.1", 0, ""});
        if (blocked(bound))
        {
            return 77;
        }
        assert(bound.ok());

        UdpAdapter client;
        assert(client.connect(TransportEndpoint {"127.0.0.1", server.local_port(), ""}).ok());
        assert(client.send(make_packet({0x01})).ok());
        assert(server.receive().ok());
        assert(client.send(make_packet({0x02})).ok());
        assert(server.receive().ok());
        assert(client.send(make_packet({0x03})).ok());
        assert(server.receive().ok());
        assert(server.stats().lost == 0 && server.stats().reordered == 0);

        UdpAdapter late_sender; // fresh sequence counter starts at 1 again
        assert(late_sender.connect(TransportEndpoint {"127.0.0.1", server.local_port(), ""}).ok());
        assert(late_sender.send(make_packet({0xEE})).ok());
        assert(server.receive().ok());
        assert(server.stats().reordered == 1); // seq 1 behind seq 3

        assert(client.close().ok());
        assert(late_sender.close().ok());
        assert(server.close().ok());
    }

    // ── Keydrop-level recovery over corrupted datagram payloads ──
    {
        SchemaRuntime runtime;
        const SchemaDef schema {
            "UdpData",
            90,
            {
                FieldDef {"value", FieldType::u16, 0, {}},
                FieldDef {"tag", FieldType::string, 1, FieldConstraints {true, 16}},
            }
        };
        assert(runtime.register_schema(schema).ok());

        NamedPayload payload;
        payload["value"] = FieldValue::from_u16(77);
        payload["tag"] = FieldValue::from_string("udp_test");

        Buffer packet;
        assert(runtime.send("UdpData", payload, packet).ok());

        // A byte stream made of noise + valid packet + noise, as would be
        // recovered from datagram payloads that were partially corrupted.
        Buffer stream;
        stream.write(0xDE);
        stream.write(0xAD);
        stream.append(packet);
        stream.write(0xBE);
        stream.write(0xEF);

        std::vector<std::pair<std::string, NamedPayload>> messages;
        usize skipped = 0;
        assert(runtime.receive_recovered_stream(stream, messages, skipped).ok());
        assert(skipped == 2); // leading noise only; trailing bytes are leftover
        assert(messages.size() == 1);
        assert(messages[0].first == "UdpData");
        assert(messages[0].second["value"].as_u16 == 77);
        assert(messages[0].second["tag"].as_string == "udp_test");
    }

    return 0;
}
