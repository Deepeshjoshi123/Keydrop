#pragma once

#include "keydrop/transport/tcp_adapter.hpp"

namespace keydrop {

class WebSocketAdapter final : public Transport {
public:
    WebSocketAdapter() = default;

    TransportKind kind() const override;
    ConnectionState state() const override;

    TransportResult connect(const TransportEndpoint& endpoint) override;
    TransportResult listen(const TransportEndpoint& endpoint) override;
    TransportResult close() override;

    TransportResult send(const Buffer& packet) override;
    TransportReceiveResult receive() override;

    TransportResult accept_connection();
    u16 local_port() const;
    const std::string& path() const;

private:
    static bool is_valid_path(const std::string& path);

    TcpAdapter tcp_;
    std::string path_ = "/";
};

}
