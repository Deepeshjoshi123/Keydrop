#pragma once

#include <string>

#include "keydrop/core/buffer.hpp"
#include "keydrop/transport/tcp_adapter.hpp"

namespace keydrop {

struct WebSocketConfig {
    usize handshake_timeout_ms = 5000;
    usize max_message_bytes = 1 << 20; // 1 MiB cap per reassembled message
};

// RFC 6455 WebSocket over the TCP adapter. Implements the opening
// handshake (HTTP Upgrade with Sec-WebSocket-Key/Accept), binary and text
// frames, client-side masking, server-side unmasking, fragmentation
// reassembly, and control frames (ping/pong/close). The Transport
// interface still exchanges whole Keydrop packets; WebSocket framing is
// internal.
// Computes the RFC 6455 Sec-WebSocket-Accept value for a given
// Sec-WebSocket-Key. Public so tests and external peers can verify the
// handshake without reimplementing SHA-1 + base64.
std::string websocket_accept_key(const std::string& key);

class WebSocketAdapter final : public Transport {
public:
    WebSocketAdapter() = default;
    explicit WebSocketAdapter(const WebSocketConfig& config);

    void configure(const WebSocketConfig& config);
    const WebSocketConfig& config() const;

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

    bool read_until_double_crlf(std::string& out);
    bool read_exact(byte* out, usize size);
    bool read_frame_header(u8& fin, u8& opcode, bool& masked, u64& payload_length, byte mask_key[4]);
    TransportResult send_frame(u8 opcode, const byte* payload, usize size);
    TransportResult send_handshake_response(const std::string& request);
    TransportResult perform_client_handshake();

    TcpAdapter tcp_;
    WebSocketConfig config_;
    std::string path_ = "/";
    bool handshake_done_ = false;
    bool client_side_ = false;

    // Receive assembly state.
    std::string inbound_;
    bool fragmented_ = false;
    u8 fragment_opcode_ = 0x0;
    std::string message_;
};

}
