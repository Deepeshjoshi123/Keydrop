// Phase 6 wire-level WebSocket tests: RFC 6455 handshake (including the
// published RFC test vector), client masking, server unmasking, ping/pong,
// close handshake, and fragmented-message reassembly — verified against
// scripted raw-TCP peers.

#include <cassert>
#include <string>

#include "keydrop/transport/tcp_adapter.hpp"
#include "keydrop/transport/websocket_adapter.hpp"
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

bool blocked(const TransportResult& result)
{
    return !result.ok()
        && (result.code == TransportStatusCode::listen_failed
            || result.code == TransportStatusCode::bind_failed)
        && result.message.find("Operation not permitted") != std::string::npos;
}

// Reads raw bytes until "\r\n\r\n" (handshake headers).
std::string raw_read_until(TcpAdapter& raw, const std::string& marker, std::string& leftover)
{
    std::string out = leftover;
    leftover.clear();
    while (out.find(marker) == std::string::npos)
    {
        const TransportReceiveResult chunk = raw.receive_raw(4096);
        assert(chunk.ok());
        out.append(reinterpret_cast<const char*>(chunk.packet.data().data()), chunk.packet.size());
    }
    const usize end = out.find(marker) + marker.size();
    leftover = out.substr(end);
    out.resize(end);
    return out;
}

std::string raw_read_exact(TcpAdapter& raw, usize size, std::string& leftover)
{
    std::string out;
    while (out.size() + leftover.size() < size)
    {
        const TransportReceiveResult chunk = raw.receive_raw(4096);
        assert(chunk.ok());
        leftover.append(reinterpret_cast<const char*>(chunk.packet.data().data()), chunk.packet.size());
    }
    out = leftover.substr(0, size);
    leftover.erase(0, size);
    return out;
}

std::string header_value(const std::string& request, const std::string& name)
{
    const std::string needle = name + ":";
    const usize start = request.find(needle);
    if (start == std::string::npos)
    {
        return "";
    }
    usize value_start = start + needle.size();
    const usize value_end = request.find("\r\n", value_start);
    while (value_start < value_end && (request[value_start] == ' ' || request[value_start] == '\t'))
    {
        ++value_start;
    }
    return request.substr(value_start, value_end - value_start);
}

void send_raw_string(TcpAdapter& raw, const std::string& text)
{
    assert(raw.send_raw(reinterpret_cast<const byte*>(text.data()), text.size()).ok());
}

// RFC 6455 §1.3 example: this key must produce this accept value.
const char* kRfcKey = "dGhlIHNhbXBsZSBub25jZQ==";
const char* kRfcAccept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

} // namespace

int main()
{
    // ── RFC test vector: accept-key computation ──────────────────
    assert(websocket_accept_key(kRfcKey) == kRfcAccept);

    // ── Our server answers a scripted raw client with the RFC vector ──
    {
        WebSocketAdapter server;
        const TransportResult listen_result = server.listen(TransportEndpoint {"127.0.0.1", 0, "/telemetry"});
        if (blocked(listen_result))
        {
            return 77;
        }
        assert(listen_result.ok());
        const u16 port = server.local_port();

        bool server_done = false;
        TestThread server_thread([&server, &server_done]() {
            assert(server.accept_connection().ok()); // performs server handshake
            server_done = true;
        });

        TcpAdapter raw_client;
        assert(raw_client.connect(TransportEndpoint {"127.0.0.1", port, ""}).ok());
        const std::string request =
            "GET /telemetry HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\n"
            "Connection: Upgrade\r\nSec-WebSocket-Key: " + std::string(kRfcKey)
            + "\r\nSec-WebSocket-Version: 13\r\n\r\n";
        send_raw_string(raw_client, request);

        std::string leftover;
        const std::string response = raw_read_until(raw_client, "\r\n\r\n", leftover);
        assert(response.find("101") != std::string::npos);
        assert(header_value(response, "Sec-WebSocket-Accept") == kRfcAccept);
        server_thread.join();
        assert(server_done);
        assert(raw_client.close().ok());
        assert(server.close().ok());
    }

    // ── Our client against a scripted raw server: masking verified ──
    {
        TcpAdapter raw_server;
        const TransportResult listen_result = raw_server.listen(TransportEndpoint {"127.0.0.1", 0, ""});
        if (blocked(listen_result))
        {
            return 77;
        }
        assert(listen_result.ok());
        const u16 port = raw_server.local_port();

        const Buffer expected_payload = make_packet({0x01, 0x02, 0x03, 0x04, 0x05});
        bool server_done = false;
        TestThread server_thread([&raw_server, &expected_payload, &server_done]() {
            assert(raw_server.accept_connection().ok());
            std::string leftover;
            const std::string request = raw_read_until(raw_server, "\r\n\r\n", leftover);
            const std::string key = header_value(request, "Sec-WebSocket-Key");
            assert(!key.empty());
            const std::string response =
                "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                "Connection: Upgrade\r\nSec-WebSocket-Accept: "
                + websocket_accept_key(key) + "\r\n\r\n";
            send_raw_string(raw_server, response);

            // Read one frame: client frames MUST be masked.
            const std::string header = raw_read_exact(raw_server, 2, leftover);
            const byte first = static_cast<byte>(header[0]);
            const byte second = static_cast<byte>(header[1]);
            assert((first & 0x0F) == 0x2 && (first & 0x80) != 0); // FIN binary
            assert((second & 0x80) != 0);                        // masked
            const usize length = second & 0x7F;
            assert(length == expected_payload.size());
            const std::string key_and_payload = raw_read_exact(raw_server, 4 + length, leftover);
            for (usize i = 0; i < length; ++i)
            {
                const byte unmasked = static_cast<byte>(key_and_payload[4 + i]) ^ static_cast<byte>(key_and_payload[i % 4]);
                assert(unmasked == expected_payload.read(i));
            }

            // Reply with an unmasked binary frame.
            std::string reply;
            reply += static_cast<char>(0x82);
            reply += static_cast<char>(length);
            reply.append(reinterpret_cast<const char*>(expected_payload.data().data()), length);
            send_raw_string(raw_server, reply);
            server_done = true;
        });

        WebSocketAdapter client;
        assert(client.connect(TransportEndpoint {"127.0.0.1", port, "/telemetry"}).ok());
        assert(client.send(expected_payload).ok());
        const TransportReceiveResult received = client.receive();
        assert(received.ok());
        assert(received.packet.data() == expected_payload.data());
        server_thread.join();
        assert(server_done);
        assert(client.close().ok());
        assert(raw_server.close().ok());
    }

    // ── Scripted raw client: ping→pong, fragmentation, close, unmasked reject ──
    {
        WebSocketAdapter server;
        const TransportResult listen_result = server.listen(TransportEndpoint {"127.0.0.1", 0, "/telemetry"});
        if (blocked(listen_result))
        {
            return 77;
        }
        assert(listen_result.ok());
        const u16 port = server.local_port();

        bool server_done = false;
        TestThread server_thread([&server, &server_done]() {
            assert(server.accept_connection().ok());

            // The unmasked client frame is rejected first.
            const TransportReceiveResult unmasked = server.receive();
            assert(!unmasked.ok() && unmasked.code == TransportStatusCode::receive_failed);

            // Fragmented text message "hello" arrives as a whole packet
            // (the interleaved ping is answered internally).
            const TransportReceiveResult message = server.receive();
            assert(message.ok());
            assert(message.packet.size() == 5);
            assert(message.packet.data()[0] == 'h' && message.packet.data()[4] == 'o');

            // Close from the peer ends the receive with an error, not data.
            const TransportReceiveResult closed = server.receive();
            assert(!closed.ok());
            server_done = true;
        });

        TcpAdapter raw_client;
        assert(raw_client.connect(TransportEndpoint {"127.0.0.1", port, ""}).ok());
        const std::string request =
            "GET /telemetry HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\n"
            "Connection: Upgrade\r\nSec-WebSocket-Key: " + std::string(kRfcKey)
            + "\r\nSec-WebSocket-Version: 13\r\n\r\n";
        send_raw_string(raw_client, request);
        std::string leftover;
        const std::string response = raw_read_until(raw_client, "\r\n\r\n", leftover);
        assert(response.find("101") != std::string::npos);

        // Unmasked frame from the client must be rejected.
        {
            std::string frame;
            frame += static_cast<char>(0x82);
            frame += '\x01';
            frame += 'x';
            send_raw_string(raw_client, frame);
        }

        // Masked ping → expect an unmasked pong echoing "hi".
        {
            const byte mask[4] = {0x11, 0x22, 0x33, 0x44};
            std::string frame;
            frame += static_cast<char>(0x89);
            frame += static_cast<char>(0x80 | 2);
            frame.append(reinterpret_cast<const char*>(mask), 4);
            frame += static_cast<char>('h' ^ mask[0]);
            frame += static_cast<char>('i' ^ mask[1]);
            send_raw_string(raw_client, frame);
            const std::string pong_header = raw_read_exact(raw_client, 2, leftover);
            assert((static_cast<byte>(pong_header[0]) & 0x0F) == 0xA); // pong, unmasked
            const usize pong_length = static_cast<byte>(pong_header[1]) & 0x7F;
            const std::string pong_payload = raw_read_exact(raw_client, pong_length, leftover);
            assert(pong_payload == "hi");
        }

        // Fragmented text message: FIN=0 "he" + FIN=1 "llo".
        {
            const byte mask[4] = {0x55, 0x66, 0x77, 0x88};
            std::string first;
            first += static_cast<char>(0x01); // no FIN, text
            first += static_cast<char>(0x80 | 2);
            first.append(reinterpret_cast<const char*>(mask), 4);
            first += static_cast<char>('h' ^ mask[0]);
            first += static_cast<char>('e' ^ mask[1]);
            send_raw_string(raw_client, first);

            std::string second;
            second += static_cast<char>(0x80); // FIN, continuation
            second += static_cast<char>(0x80 | 3);
            second.append(reinterpret_cast<const char*>(mask), 4);
            second += static_cast<char>('l' ^ mask[0]);
            second += static_cast<char>('l' ^ mask[1]);
            second += static_cast<char>('o' ^ mask[2]);
            send_raw_string(raw_client, second);
        }

        // Masked close → expect a close reply.
        {
            const byte mask[4] = {1, 2, 3, 4};
            std::string frame;
            frame += static_cast<char>(0x88);
            frame += static_cast<char>(0x80);
            frame.append(reinterpret_cast<const char*>(mask), 4);
            send_raw_string(raw_client, frame);
            const std::string close_header = raw_read_exact(raw_client, 2, leftover);
            assert((static_cast<byte>(close_header[0]) & 0x0F) == 0x8);
        }

        server_thread.join();
        assert(server_done);
        assert(raw_client.close().ok());
        assert(server.close().ok());
    }

    return 0;
}
