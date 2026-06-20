#include <cassert>
#include <string>
#include "keydrop/transport/transport_scheduler.hpp"
#include "keydrop/transport/websocket_adapter.hpp"
#include "test_thread.hpp"

using namespace keydrop;

static Buffer make_packet(std::initializer_list<byte> bytes)
{
    Buffer packet;
    for (byte value : bytes)
    {
        packet.write(value);
    }
    return packet;
}

int main()
{
    {
        WebSocketAdapter adapter;
        assert(adapter.kind() == TransportKind::websocket);
        assert(adapter.state() == ConnectionState::disconnected);

        const TransportEndpoint bad_path {"127.0.0.1", 9001, "telemetry"};
        const TransportResult bad_connect = adapter.connect(bad_path);
        assert(bad_connect.code == TransportStatusCode::invalid_endpoint);
    }

    WebSocketAdapter server;
    const TransportEndpoint listen_endpoint {"127.0.0.1", 0, "/telemetry"};
    const TransportResult listen_result = server.listen(listen_endpoint);
    if (
        !listen_result.ok()
        &&
        (
            listen_result.code == TransportStatusCode::listen_failed
            ||
            listen_result.code == TransportStatusCode::bind_failed
        )
        &&
        listen_result.message.find("Operation not permitted") != std::string::npos
    )
    {
        return 77;
    }
    assert(listen_result.ok());
    assert(server.path() == "/telemetry");
    assert(server.local_port() != 0);

    bool server_done = false;
    TestThread server_thread([&server, &server_done]() {
        const TransportResult accept_result = server.accept_connection();
        assert(accept_result.ok());

        const TransportReceiveResult received = server.receive();
        assert(received.ok());
        assert(received.packet.size() == 2);
        assert(received.packet.read(0) == 0xAA);
        assert(received.packet.read(1) == 0xBB);

        Buffer reply = make_packet({0x11});
        assert(server.send(reply).ok());
        server_done = true;
    });

    WebSocketAdapter client;
    const TransportEndpoint connect_endpoint {"127.0.0.1", server.local_port(), "/telemetry"};
    const TransportResult connect_result = client.connect(connect_endpoint);
    assert(connect_result.ok());
    assert(client.kind() == TransportKind::websocket);
    assert(client.path() == "/telemetry");

    TransportScheduler scheduler;
    scheduler.enqueue(make_packet({0xAA, 0xBB}));
    const SchedulerResult flush_result = scheduler.flush(client);
    assert(flush_result.ok());
    assert(flush_result.packets_sent == 1);
    assert(flush_result.bytes_sent == 2);

    const TransportReceiveResult reply = client.receive();
    assert(reply.ok());
    assert(reply.packet.size() == 1);
    assert(reply.packet.read(0) == 0x11);

    server_thread.join();
    assert(server_done);

    assert(client.close().ok());
    assert(server.close().ok());

    return 0;
}
