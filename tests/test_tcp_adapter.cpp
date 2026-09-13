#include <cassert>
#include <string>
#include "keydrop/transport/tcp_adapter.hpp"
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
        TcpAdapter adapter;
        Buffer packet = make_packet({0x01});
        assert(adapter.kind() == TransportKind::tcp);
        assert(adapter.state() == ConnectionState::disconnected);
        assert(adapter.send(packet).code == TransportStatusCode::not_connected);
        assert(adapter.receive().code == TransportStatusCode::not_connected);

        const TransportEndpoint invalid_endpoint {"", 9001, ""};
        const TransportResult invalid_listen = adapter.listen(invalid_endpoint);
        assert(invalid_listen.code == TransportStatusCode::invalid_endpoint);
        assert(adapter.state() == ConnectionState::failed);
        assert(adapter.close().ok());
    }

    TcpAdapter server;
    const TransportEndpoint listen_endpoint {"127.0.0.1", 0, ""};
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
    assert(server.state() == ConnectionState::listening);
    assert(server.local_port() != 0);

    bool server_done = false;
    TestThread server_thread([&server, &server_done]() {
        const TransportResult accept_result = server.accept_connection();
        assert(accept_result.ok());
        assert(server.state() == ConnectionState::connected);

        const TransportReceiveResult received = server.receive();
        assert(received.ok());
        assert(received.packet.size() == 3);
        assert(received.packet.read(0) == 0xAA);
        assert(received.packet.read(1) == 0xBB);
        assert(received.packet.read(2) == 0xCC);

        Buffer reply = make_packet({0x10, 0x20});
        const TransportResult send_reply = server.send(reply);
        assert(send_reply.ok());
        assert(send_reply.bytes_transferred == 2);
        server_done = true;
    });

    TcpAdapter client;
    const TransportEndpoint connect_endpoint {"127.0.0.1", server.local_port(), ""};
    const TransportResult connect_result = client.connect(connect_endpoint);
    assert(connect_result.ok());
    assert(client.state() == ConnectionState::connected);

    Buffer packet = make_packet({0xAA, 0xBB, 0xCC});
    const TransportResult send_result = client.send(packet);
    assert(send_result.ok());
    assert(send_result.bytes_transferred == 3);

    const TransportReceiveResult reply = client.receive();
    assert(reply.ok());
    assert(reply.packet.size() == 2);
    assert(reply.packet.read(0) == 0x10);
    assert(reply.packet.read(1) == 0x20);

    server_thread.join();
    assert(server_done);

    assert(client.close().ok());
    assert(server.close().ok());
    assert(client.state() == ConnectionState::disconnected);
    assert(server.state() == ConnectionState::disconnected);

    return 0;
}
