#include <cassert>

#include "keydrop/transport/transport_config.hpp"
#include "keydrop/transport/transport_scheduler.hpp"

using namespace keydrop;

class RecordingTransport final : public Transport {
public:
    TransportKind kind() const override
    {
        return TransportKind::tcp;
    }

    ConnectionState state() const override
    {
        return connected_ ? ConnectionState::connected : ConnectionState::disconnected;
    }

    TransportResult connect(const TransportEndpoint&) override
    {
        connected_ = true;
        return {TransportStatusCode::ok, "Connected.", 0};
    }

    TransportResult listen(const TransportEndpoint&) override
    {
        connected_ = true;
        return {TransportStatusCode::ok, "Listening.", 0};
    }

    TransportResult close() override
    {
        connected_ = false;
        return {TransportStatusCode::ok, "Closed.", 0};
    }

    TransportResult send(const Buffer& packet) override
    {
        if (!connected_)
        {
            return {TransportStatusCode::not_connected, "Not connected.", 0};
        }

        sent_packets += 1;
        sent_bytes += packet.size();
        return {TransportStatusCode::ok, "Sent.", packet.size()};
    }

    TransportReceiveResult receive() override
    {
        return {TransportStatusCode::unsupported, "Receive not supported.", {}};
    }

    bool connected_ = false;
    usize sent_packets = 0;
    usize sent_bytes = 0;
};

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
        TransportConfig config;
        config.kind = TransportKind::tcp;
        config.endpoint = {"127.0.0.1", 9001, ""};
        TransportCreateResult created = create_transport(config);
        assert(created.ok());
        assert(created.transport->kind() == TransportKind::tcp);
    }

    {
        TransportConfig config;
        config.kind = TransportKind::websocket;
        config.endpoint = {"127.0.0.1", 9001, "/telemetry"};
        TransportCreateResult created = create_transport(config);
        assert(created.ok());
        assert(created.transport->kind() == TransportKind::websocket);
    }

    {
        TransportConfig config;
        config.endpoint = {"", 9001, ""};
        TransportCreateResult created = create_transport(config);
        assert(!created.ok());
        assert(created.code == TransportStatusCode::invalid_endpoint);
    }

    TransportScheduler scheduler;
    assert(scheduler.empty());

    RecordingTransport transport;
    scheduler.enqueue(make_packet({0x01, 0x02}));
    scheduler.enqueue(make_packet({0x03}));
    assert(scheduler.pending() == 2);

    SchedulerResult failed = scheduler.flush(transport);
    assert(!failed.ok());
    assert(failed.code == SchedulerStatusCode::send_failed);
    assert(failed.packets_failed == 2);
    assert(scheduler.pending() == 2);

    assert(transport.connect({"127.0.0.1", 9001, ""}).ok());
    SchedulerResult flushed = scheduler.flush(transport);
    assert(flushed.ok());
    assert(flushed.packets_sent == 2);
    assert(flushed.bytes_sent == 3);
    assert(scheduler.empty());
    assert(transport.sent_packets == 2);
    assert(transport.sent_bytes == 3);

    SchedulerResult empty = scheduler.flush(transport);
    assert(empty.code == SchedulerStatusCode::empty);

    return 0;
}
