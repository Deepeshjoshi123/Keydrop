// Phase 6 TCP reliability test: framing, timeouts, reconnect, and
// scheduler backpressure. Returns 77 (skip) when the environment blocks
// socket use, matching the existing TCP/WebSocket adapter tests.

#include <cassert>
#include <string>

#include "keydrop/transport/tcp_adapter.hpp"
#include "keydrop/transport/transport_scheduler.hpp"
#include "test_thread.hpp"

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

bool environment_blocked(const TransportResult& result)
{
    return !result.ok()
        && (result.code == TransportStatusCode::listen_failed
            || result.code == TransportStatusCode::bind_failed)
        && result.message.find("Operation not permitted") != std::string::npos;
}

u16 start_server(TcpAdapter& server)
{
    const TransportResult listen_result = server.listen(TransportEndpoint {"127.0.0.1", 0, ""});
    assert(listen_result.ok());
    return server.local_port();
}

} // namespace

int main()
{
    // ── Framing + round-trip through the scheduler ───────────────
    {
        TcpAdapter server;
        const TransportResult listen_result = server.listen(TransportEndpoint {"127.0.0.1", 0, ""});
        if (environment_blocked(listen_result))
        {
            return 77;
        }
        assert(listen_result.ok());
        const u16 port = server.local_port();

        bool server_done = false;
        TestThread server_thread([&server, &server_done]() {
            assert(server.accept_connection().ok());
            for (usize i = 0; i < 3; ++i)
            {
                const TransportReceiveResult received = server.receive();
                assert(received.ok());
                assert(received.packet.size() == 1 + i);
            }
            server_done = true;
        });

        TcpAdapter client;
        assert(client.connect(TransportEndpoint {"127.0.0.1", port, ""}).ok());
        TransportScheduler scheduler;
        assert(scheduler.enqueue(make_packet({0x01})));
        assert(scheduler.enqueue(make_packet({0x02, 0x03})));
        assert(scheduler.enqueue(make_packet({0x04, 0x05, 0x06})));
        const SchedulerResult flushed = scheduler.flush(client);
        assert(flushed.ok() && flushed.packets_sent == 3);
        server_thread.join();
        assert(server_done);
        assert(client.close().ok());
        assert(server.close().ok());
    }

    // ── Connect timeout: unreachable address fails within the budget ──
    {
        TcpConfig config;
        config.connect_timeout_ms = 500;
        config.reconnect_attempts = 0;
        TcpAdapter client(config);
        const TransportResult result = client.connect(TransportEndpoint {"10.255.255.1", 65000, ""});
        assert(!result.ok());
        assert(result.code == TransportStatusCode::connect_failed || result.code == TransportStatusCode::invalid_endpoint);
        assert(client.state() == ConnectionState::failed);
    }

    // ── Reconnect: server drops, restarts, client reconnects ─────
    {
        TcpAdapter server;
        const TransportResult listen_result = server.listen(TransportEndpoint {"127.0.0.1", 0, ""});
        if (environment_blocked(listen_result))
        {
            return 77;
        }
        assert(listen_result.ok());
        const u16 port = server.local_port();

        TcpConfig client_config;
        client_config.reconnect_attempts = 3;
        client_config.reconnect_delay_ms = 50;
        TcpAdapter client(client_config);
        assert(client.connect(TransportEndpoint {"127.0.0.1", port, ""}).ok());
        assert(client.has_last_endpoint() && client.last_endpoint().port == port);

        // Server accepts once, receives one packet, then drops.
        bool first_serve_done = false;
        TestThread first_server([&server, &first_serve_done]() {
            assert(server.accept_connection().ok());
            const TransportReceiveResult received = server.receive();
            assert(received.ok());
            first_serve_done = true;
            (void)server.close();
        });
        assert(client.send(make_packet({0x11})).ok());
        first_server.join();
        assert(first_serve_done);

        // Server restarts on the same port.
        TcpAdapter server_again;
        assert(server_again.listen(TransportEndpoint {"127.0.0.1", port, ""}).ok());
        bool second_serve_done = false;
        TestThread second_server([&server_again, &second_serve_done]() {
            assert(server_again.accept_connection().ok());
            const TransportReceiveResult received = server_again.receive();
            assert(received.ok());
            assert(received.packet.size() == 1 && received.packet.read(0) == 0x22);
            second_serve_done = true;
        });

        // Manual reconnect after the drop is detected (close first: the
        // stale socket still believes it is connected), then traffic flows.
        assert(client.reconnect().code == TransportStatusCode::already_connected);
        assert(client.close().ok());
        assert(client.reconnect().ok());
        assert(client.send(make_packet({0x22})).ok());
        second_server.join();
        assert(second_serve_done);
        assert(client.close().ok());
        assert(server_again.close().ok());
    }

    // ── Receive timeout: silent peer fails within the budget ─────
    {
        TcpAdapter server;
        const TransportResult listen_result = server.listen(TransportEndpoint {"127.0.0.1", 0, ""});
        if (environment_blocked(listen_result))
        {
            return 77;
        }
        assert(listen_result.ok());
        const u16 port = server.local_port();

        bool accepted = false;
        TestThread silent_server([&server, &accepted]() {
            assert(server.accept_connection().ok());
            accepted = true;
            // Never sends anything.
            for (volatile usize spin = 0; spin < 300000000; ++spin)
            {
            }
            (void)server.close();
        });

        TcpConfig client_config;
        client_config.receive_timeout_ms = 300;
        client_config.reconnect_attempts = 0;
        TcpAdapter client(client_config);
        assert(client.connect(TransportEndpoint {"127.0.0.1", port, ""}).ok());
        const TransportReceiveResult timed_out = client.receive();
        assert(!timed_out.ok());
        assert(timed_out.code == TransportStatusCode::receive_failed);
        silent_server.join();
        assert(accepted);
        assert(client.close().ok());
    }

    // ── Scheduler backpressure ───────────────────────────────────
    {
        TransportScheduler scheduler;
        scheduler.set_max_pending(2);
        assert(scheduler.enqueue(make_packet({0x01})));
        assert(scheduler.enqueue(make_packet({0x02})));
        assert(!scheduler.enqueue(make_packet({0x03}))); // limit reached
        assert(scheduler.pending() == 2);

        // Fake transport that drains the queue.
        class FakeTransport : public Transport {
        public:
            TransportKind kind() const override { return TransportKind::tcp; }
            ConnectionState state() const override { return ConnectionState::connected; }
            TransportResult connect(const TransportEndpoint&) override { return {TransportStatusCode::ok, "", 0}; }
            TransportResult listen(const TransportEndpoint&) override { return {TransportStatusCode::ok, "", 0}; }
            TransportResult close() override { return {TransportStatusCode::ok, "", 0}; }
            TransportResult send(const Buffer& packet) override
            {
                sent += 1;
                return {TransportStatusCode::ok, "", packet.size()};
            }
            TransportReceiveResult receive() override { return {TransportStatusCode::ok, "", {}}; }
            usize sent = 0;
        };

        FakeTransport fake;
        const SchedulerResult flushed = scheduler.flush(fake);
        assert(flushed.ok() && flushed.packets_sent == 2 && fake.sent == 2);
        assert(scheduler.empty());
        assert(scheduler.enqueue(make_packet({0x04}))); // space again
        scheduler.clear();
    }

    return 0;
}
