#pragma once

#include <cstdint>

#include "keydrop/transport/transport.hpp"

namespace keydrop {

// Phase 6 TCP completion settings. Timeouts of 0 mean blocking (platform
// default). reconnect_attempts > 0 makes send()/receive() attempt an
// automatic reconnect to the last endpoint when the connection fails.
struct TcpConfig {
    usize connect_timeout_ms = 5000;
    usize send_timeout_ms = 5000;
    usize receive_timeout_ms = 5000;
    usize reconnect_attempts = 0;
    usize reconnect_delay_ms = 100;
};

class TcpAdapter final : public Transport {
public:
    TcpAdapter() = default;
    explicit TcpAdapter(const TcpConfig& config);
    ~TcpAdapter() override;

    TcpAdapter(const TcpAdapter&) = delete;
    TcpAdapter& operator=(const TcpAdapter&) = delete;

    void configure(const TcpConfig& config);
    const TcpConfig& config() const;

    TransportKind kind() const override;
    ConnectionState state() const override;

    TransportResult connect(const TransportEndpoint& endpoint) override;
    TransportResult listen(const TransportEndpoint& endpoint) override;
    TransportResult close() override;

    TransportResult send(const Buffer& packet) override;
    TransportReceiveResult receive() override;

    TransportResult accept_connection();
    u16 local_port() const;

    // Raw socket access without Keydrop length framing. Used by protocols
    // with their own framing (WebSocket frames, MQTT packets). receive_raw
    // returns whatever a single recv produced (empty = timeout/failure).
    TransportResult send_raw(const byte* data, usize size);
    TransportReceiveResult receive_raw(usize max_bytes);

    // Manual reconnect to the last endpoint used by connect(). Also invoked
    // automatically by send()/receive() when reconnect_attempts > 0.
    TransportResult reconnect();
    const TransportEndpoint& last_endpoint() const;
    bool has_last_endpoint() const;

private:
    std::uintptr_t active_socket() const;
    void close_socket(std::uintptr_t& socket_fd);
    void apply_timeouts(std::uintptr_t socket);
    bool wait_until_writable(std::uintptr_t socket, usize timeout_ms);
    bool ensure_connected();

    std::uintptr_t socket_fd_ = static_cast<std::uintptr_t>(-1);
    std::uintptr_t peer_fd_ = static_cast<std::uintptr_t>(-1);
    u16 local_port_ = 0;
    ConnectionState state_ = ConnectionState::disconnected;
    TcpConfig config_;
    TransportEndpoint last_endpoint_;
    bool has_last_endpoint_ = false;
};

}
