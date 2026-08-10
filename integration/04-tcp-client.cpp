/// 04-tcp-client.cpp — TCP transport: client side
///
/// What you learn:
///  1. Create a TcpAdapter and connect to a server
///  2. Encode messages with the schema runtime
///  3. Send Keydrop-encoded packets over TCP
///
/// Build:  see CMakeLists.txt in this directory
/// Run:    ./build/integration/04-tcp-client
///         (run 04-tcp-server in a second terminal first)

#include <cassert>
#include <iostream>
#include <string>
#include <thread>

#include "keydrop/schema/schema_runtime.hpp"
#include "keydrop/transport/tcp_adapter.hpp"

using namespace keydrop;

int main()
{
    // ── 1. Register schema ─────────────────────────────────────
    SchemaRuntime runtime;
    runtime.register_schema(SchemaDef{"Ping", 1, {
        FieldDef{"seq",   FieldType::u32,    0, {}},
        FieldDef{"label", FieldType::string, 1, FieldConstraints{true, 64}},
    }});

    // ── 2. Connect to server ───────────────────────────────────
    TcpAdapter client;
    TransportEndpoint endpoint{"127.0.0.1", 9876, ""};

    // Small delay to let server start (when launched by demo script)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    TransportResult conn_res = client.connect(endpoint);
    assert(conn_res.ok());
    std::cout << "[client] connected to " << endpoint.host
              << ":" << endpoint.port << "\n";

    // ── 3. Encode and send messages ────────────────────────────
    for (u32 seq = 0; seq < 3; ++seq) {
        NamedPayload payload;
        payload["seq"]   = FieldValue::from_u32(seq);
        payload["label"] = FieldValue::from_string("hello-tcp");

        Buffer packet;
        runtime.send("Ping", payload, packet);

        TransportResult send_res = client.send(packet);
        assert(send_res.ok());
        std::cout << "[client] sent: seq=" << seq
                  << "  (" << packet.size() << " B)\n";
    }

    client.close();
    std::cout << "\n[pass] client done\n";
    return 0;
}
