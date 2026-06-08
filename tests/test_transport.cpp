#include <cassert>

#include "keydrop/transport/transport.hpp"

using namespace keydrop;

class MemoryTransport final : public Transport {
public:
    TransportKind kind() const override
    {
        return TransportKind::tcp;
    }

    ConnectionState state() const override
    {
        return state_;
    }

    TransportResult connect(const TransportEndpoint& endpoint) override
    {
        if (!is_valid_endpoint(endpoint))
        {
            state_ = ConnectionState::failed;
            return {TransportStatusCode::invalid_endpoint, "Invalid endpoint.", 0};
        }

        if (state_ == ConnectionState::connected)
        {
            return {TransportStatusCode::already_connected, "Already connected.", 0};
        }

        endpoint_ = endpoint;
        state_ = ConnectionState::connected;
        return {TransportStatusCode::ok, "Connected.", 0};
    }

    TransportResult listen(const TransportEndpoint& endpoint) override
    {
        if (!is_valid_endpoint(endpoint))
        {
            state_ = ConnectionState::failed;
            return {TransportStatusCode::invalid_endpoint, "Invalid endpoint.", 0};
        }

        endpoint_ = endpoint;
        state_ = ConnectionState::listening;
        return {TransportStatusCode::ok, "Listening.", 0};
    }

    TransportResult close() override
    {
        state_ = ConnectionState::disconnected;
        packet_.clear();
        return {TransportStatusCode::ok, "Closed.", 0};
    }

    TransportResult send(const Buffer& packet) override
    {
        if (state_ != ConnectionState::connected && state_ != ConnectionState::listening)
        {
            return {TransportStatusCode::not_connected, "Transport is not connected.", 0};
        }

        packet_ = packet;
        return {TransportStatusCode::ok, "Packet sent.", packet.size()};
    }

    TransportReceiveResult receive() override
    {
        if (state_ != ConnectionState::connected && state_ != ConnectionState::listening)
        {
            return {TransportStatusCode::not_connected, "Transport is not connected.", {}};
        }

        return {TransportStatusCode::ok, "Packet received.", packet_};
    }

private:
    ConnectionState state_ = ConnectionState::disconnected;
    TransportEndpoint endpoint_;
    Buffer packet_;
};

int main()
{
    assert(transport_kind_to_string(TransportKind::tcp)[0] == 't');
    assert(transport_kind_to_string(TransportKind::websocket)[0] == 'w');
    assert(connection_state_to_string(ConnectionState::connected)[0] == 'c');
    assert(transport_status_to_string(TransportStatusCode::not_connected)[0] == 'n');

    const TransportEndpoint valid_endpoint {"127.0.0.1", 9001, ""};
    const TransportEndpoint invalid_endpoint {"", 0, ""};
    assert(is_valid_endpoint(valid_endpoint));
    assert(!is_valid_endpoint(invalid_endpoint));

    MemoryTransport transport;
    assert(transport.kind() == TransportKind::tcp);
    assert(transport.state() == ConnectionState::disconnected);

    Buffer packet;
    packet.write(0xAA);
    assert(transport.send(packet).code == TransportStatusCode::not_connected);
    assert(transport.receive().code == TransportStatusCode::not_connected);

    const TransportResult invalid_connect = transport.connect(invalid_endpoint);
    assert(!invalid_connect.ok());
    assert(invalid_connect.code == TransportStatusCode::invalid_endpoint);
    assert(transport.state() == ConnectionState::failed);

    const TransportResult connect_result = transport.connect(valid_endpoint);
    assert(connect_result.ok());
    assert(transport.state() == ConnectionState::connected);

    const TransportResult duplicate_connect = transport.connect(valid_endpoint);
    assert(duplicate_connect.code == TransportStatusCode::already_connected);

    const TransportResult send_result = transport.send(packet);
    assert(send_result.ok());
    assert(send_result.bytes_transferred == 1);

    const TransportReceiveResult receive_result = transport.receive();
    assert(receive_result.ok());
    assert(receive_result.packet.size() == 1);
    assert(receive_result.packet.read(0) == 0xAA);

    assert(transport.close().ok());
    assert(transport.state() == ConnectionState::disconnected);

    assert(transport.listen(valid_endpoint).ok());
    assert(transport.state() == ConnectionState::listening);

    return 0;
}
