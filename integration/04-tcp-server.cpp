/// 04-tcp-server.cpp — TCP transport: server side
///
/// What you learn:
///  1. Create a TcpAdapter, bind, and listen
///  2. Accept a client connection
///  3. Receive Keydrop-encoded packets over TCP
///  4. Decode them with the schema runtime
///
/// Build:  see CMakeLists.txt in this directory
/// Run:    ./build/integration/04-tcp-server
///         (run 04-tcp-client in a second terminal)

#include <cassert>
#include <iostream>
#include <string>

#include "keydrop/schema/schema_runtime.hpp"
#include "keydrop/transport/tcp_adapter.hpp"

using namespace keydrop;

int main()
{
    // ── 1. Register the same schema the client will use ────────
    SchemaRuntime runtime;
    runtime.register_schema(SchemaDef{"Ping", 1, {
        FieldDef{"seq",   FieldType::u32,    0, {}},
        FieldDef{"label", FieldType::string, 1, FieldConstraints{true, 64}},
    }});

    // ── 2. Bind and listen ─────────────────────────────────────
    TcpAdapter server;
    TransportEndpoint endpoint{"127.0.0.1", 9876, ""};
    TransportResult listen_res = server.listen(endpoint);
    assert(listen_res.ok());
    std::cout << "[server] listening on " << endpoint.host
              << ":" << endpoint.port << "\n";

    // ── 3. Accept one client ───────────────────────────────────
    TransportResult accept_res = server.accept_connection();
    assert(accept_res.ok());
    std::cout << "[server] client connected\n";

    // ── 4. Receive and decode packets ──────────────────────────
    for (int i = 0; i < 3; ++i) {
        TransportReceiveResult recv = server.receive();
        if (!recv.ok()) {
            std::cerr << "[server] receive error: " << recv.message << "\n";
            break;
        }

        std::string schema_name;
        NamedPayload decoded;
        SchemaRuntimeResult decode_res = runtime.receive(recv.packet, schema_name, decoded);
        assert(decode_res.ok());

        std::cout << "[server] received: seq=" << decoded["seq"].as_u32
                  << "  label=" << decoded["label"].as_string
                  << "  (" << recv.packet.size() << " B)\n";
    }

    server.close();
    std::cout << "\n[pass] server done\n";
    return 0;
}
