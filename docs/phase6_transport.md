# Phase 6: Transport Scope

TCP, WebSocket, and UDP are implemented. MQTT is future scope (see below).

## Release-order status

| Item | Status |
|---|---|
| TCP | **Done**: framing, partial reads/writes, reconnect, timeouts, backpressure, security strategy documented. |
| WebSocket | **Done (real RFC 6455)**: HTTP Upgrade handshake with SHA-1/base64 accept-key computation, binary/text frames, client masking, server unmasking, ping/pong/close control frames, fragmented-message reassembly, and a configurable message-size cap. |
| UDP | **Done**: datagram transport with per-datagram sequence numbers, loss/reordering/duplicate detection via `stats()`, an MTU bound, and a test-only loss hook (`drop_sequence`). Reliability (retransmission) is intentionally out of scope — streams recover through Keydrop keyframes (Phase 3) and `receive_recovered_stream` (Phase 5). |
| MQTT | **Future scope.** A broker-based IoT use case has not been defined for this repository yet; the plan defers MQTT until then. The raw-socket access added to `TcpAdapter` (`send_raw`/`receive_raw`) is the groundwork a future MQTT client adapter would build on. |

## TCP completion

`TcpConfig`: `connect_timeout_ms`, `send_timeout_ms`, `receive_timeout_ms`
(0 = blocking), `reconnect_attempts`, `reconnect_delay_ms`. Non-blocking
`connect()` bounded by the timeout (select-based, POSIX/Winsock);
`SO_SNDTIMEO`/`SO_RCVTIMEO` bound send/receive; `reconnect()` and automatic
reconnect on failed send/receive; `TransportScheduler::set_max_pending(n)`
bounds the queue (backpressure).

## WebSocket (RFC 6455)

```cpp
WebSocketConfig config;
config.max_message_bytes = 1 << 20; // per reassembled message
WebSocketAdapter ws(config);
ws.connect({"127.0.0.1", 8080, "/telemetry"});
ws.send(packet);          // binary frame (masked on the client side)
auto received = ws.receive(); // whole message, control frames handled internally
```

- Client performs the HTTP Upgrade and validates `Sec-WebSocket-Accept`
  against SHA-1(key + GUID) — verified against the RFC 6455 §1.3 test
  vector (`dGhlIHNhbXBsZSBub25jZQ==` → `s3pPLMBiTxaQ9kYGzzhZRbK+xOo=`).
- Server validates the upgrade request and answers 101; unmasked client
  frames are rejected (after consuming the payload, so a rejected frame
  can never desynchronize the stream).
- Ping → pong, close → close reply + teardown, fragmented messages are
  reassembled into one Transport message; 16-bit and 64-bit frame lengths
  are supported.
- `websocket_accept_key(key)` is public so external peers/tests can
  verify the handshake without reimplementing SHA-1 + base64.

## UDP

```cpp
UdpConfig config;
config.max_datagram_bytes = 1200; // MTU-safe: 6-byte framing + payload
config.receive_timeout_ms = 5000;
UdpAdapter udp(config);
udp.listen({"127.0.0.1", 0, ""});  // or connect() to set the peer
udp.send(packet);                  // [seq u32][len u16][payload] datagram
udp.receive();
udp.stats();                       // lost / reordered / received counters
```

- Packets larger than `max_datagram_bytes - 6` are rejected (MTU bound).
- A bound (unconnected) socket replies to the last received source via
  `sendto`; a connected socket filters by peer.
- Loss and reordering are **detected, not retransmitted**: sequence gaps
  and behind-sequence arrivals are counted in `stats()`. Recovery is a
  Keydrop-layer concern (keyframes, `receive_recovered_stream`).

## Security strategy (all transports)

Framing plus optional Phase 5 CRC32 envelopes give integrity and strict
corruption rejection. Confidentiality, authentication, and replay
protection are deployment-layer concerns (e.g., TLS/VPN in front of the
adapter) and are deliberately out of scope.

## Byte accounting

`transport_bytes_benchmark` reports application payload, Keydrop framing,
and transport framing separately (fixed record: 13 B payload / 4 B Keydrop
framing / 4 B TCP length prefix = 21 B wire).

## Verification

| Release gate | Result |
|---|---|
| TCP: framing, reconnect, backpressure | 34/34 ctest pass, including `test_tcp_reliability` (framing round-trip, connect timeout, manual reconnect, receive timeout, scheduler backpressure) |
| WebSocket: handshake, framing, masking, control frames | `test_websocket_handshake` (RFC test vector, scripted raw peers both directions, masking enforced, ping/pong, fragmented reassembly, close handshake, unmasked rejection) + `test_websocket_adapter` (scheduler round-trip over real frames) |
| UDP: MTU, sequencing, loss, reordering, recovery | `test_udp_adapter` (round-trip both directions, MTU rejection, sequence gap detection via the loss hook, reordering detection, Keydrop-level recovery over corrupted payloads) |

## Limitations

- IPv4 only (`inet_addr`/`AF_INET`); no hostname resolution — pass an IP.
- Blocking sockets, single-connection adapters; no poll-based
  multiplexing.
- No TLS in the adapters; see the security strategy above.
- WebSocket text frames are delivered as binary `Buffer` messages (no
  UTF-8 validation); WebSocket extensions (permessage-deflate etc.) are
  not negotiated.
- UDP reliability is detection-only; use stream mode with keyframes for
  loss-tolerant delivery.
- MQTT, UDP broadcast/multicast, and QUIC are future scope.
