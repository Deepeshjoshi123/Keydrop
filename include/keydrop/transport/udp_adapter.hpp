#pragma once

#include <cstdint>

#include "keydrop/transport/transport.hpp"

namespace keydrop {

struct UdpConfig {
    // Datagram bound: 6-byte framing + payload must fit within this so the
    // datagram stays under common path MTUs (default 1200 is conservative).
    usize max_datagram_bytes = 1200;
    usize receive_timeout_ms = 5000; // 0 = blocking
    // Test hook: when nonzero, send() silently drops the datagram with
    // this sequence number (simulating wire loss) while advancing the
    // sequence. 0 = disabled.
    u32 drop_sequence = 0;
};

// Per-socket sequence statistics. UDP is unordered and lossy; this adapter
// detects (does not retransmit) loss, reordering, and duplicates.
struct UdpStats {
    u32 last_sequence = 0;
    bool has_sequence = false;
    u64 datagrams_sent = 0;
    u64 datagrams_received = 0;
    u64 lost = 0;       // gaps detected in the received sequence
    u64 reordered = 0;  // datagrams arriving behind the last sequence
};

// Datagram transport with sequencing. Each send() emits one datagram:
// [seq u32 LE][length u16 LE][payload]. Packets larger than
// max_datagram_bytes - 6 are rejected. Reliability (retransmission) is
// intentionally out of scope: streams recover through Keydrop keyframes
// and receive_recovered_stream (Phases 3 and 5); the sequence numbers here
// make loss and reordering observable via stats().
class UdpAdapter final : public Transport {
public:
    UdpAdapter() = default;
    explicit UdpAdapter(const UdpConfig& config);
    ~UdpAdapter() override;

    UdpAdapter(const UdpAdapter&) = delete;
    UdpAdapter& operator=(const UdpAdapter&) = delete;

    void configure(const UdpConfig& config);
    const UdpConfig& config() const;

    TransportKind kind() const override;
    ConnectionState state() const override;

    // connect() sets the remote peer (and filters receives to it via the
    // connected UDP socket). listen() binds the local port.
    TransportResult connect(const TransportEndpoint& endpoint) override;
    TransportResult listen(const TransportEndpoint& endpoint) override;
    TransportResult close() override;

    TransportResult send(const Buffer& packet) override;
    TransportReceiveResult receive() override;

    u16 local_port() const;
    const UdpStats& stats() const;
    void reset_stats();

private:
    UdpConfig config_;
    std::uintptr_t socket_fd_ = static_cast<std::uintptr_t>(-1);
    u16 local_port_ = 0;
    ConnectionState state_ = ConnectionState::disconnected;
    u32 next_sequence_ = 1;
    UdpStats stats_;
    // Source of the last received datagram (for replies from an
    // unconnected/bound socket via sendto).
    bool peer_set_ = false;
    u32 peer_addr_ = 0;
    u16 peer_port_ = 0;
};

}
