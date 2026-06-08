#pragma once

#include "keydrop/transport/transport.hpp"

namespace keydrop {

class TcpAdapter final : public Transport {
public:
    TcpAdapter() = default;
    ~TcpAdapter() override;

    TcpAdapter(const TcpAdapter&) = delete;
    TcpAdapter& operator=(const TcpAdapter&) = delete;

    TransportKind kind() const override;
    ConnectionState state() const override;

    TransportResult connect(const TransportEndpoint& endpoint) override;
    TransportResult listen(const TransportEndpoint& endpoint) override;
    TransportResult close() override;

    TransportResult send(const Buffer& packet) override;
    TransportReceiveResult receive() override;

    TransportResult accept_connection();
    u16 local_port() const;

private:
    int active_socket() const;
    void close_socket(int& socket_fd);

    int socket_fd_ = -1;
    int peer_fd_ = -1;
    u16 local_port_ = 0;
    ConnectionState state_ = ConnectionState::disconnected;
};

}
