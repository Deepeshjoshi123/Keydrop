#include "keydrop/transport/websocket_adapter.hpp"

#include <cstring>
#include <ctime>

namespace keydrop {

namespace {

constexpr const char* kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
constexpr usize kMaxHandshakeBytes = 8192;

// ── Minimal SHA-1 (RFC 3174) — the only hash the handshake needs ──
struct Sha1 {
    u32 state[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    u64 total = 0;
    byte block[64];
    usize block_used = 0;

    static u32 rol(u32 v, usize bits) { return (v << bits) | (v >> (32 - bits)); }

    void process_block(const byte* p)
    {
        u32 w[80];
        for (usize i = 0; i < 16; ++i)
        {
            w[i] = static_cast<u32>(p[i * 4]) << 24
                | static_cast<u32>(p[i * 4 + 1]) << 16
                | static_cast<u32>(p[i * 4 + 2]) << 8
                | static_cast<u32>(p[i * 4 + 3]);
        }
        for (usize i = 16; i < 80; ++i)
        {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        u32 a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
        for (usize i = 0; i < 80; ++i)
        {
            u32 f = 0;
            u32 k = 0;
            if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
            const u32 temp = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = temp;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
    }

    void update(const byte* data, usize size)
    {
        total += size;
        while (size > 0)
        {
            const usize take = 64 - block_used < size ? 64 - block_used : size;
            std::memcpy(block + block_used, data, take);
            block_used += take;
            data += take;
            size -= take;
            if (block_used == 64)
            {
                process_block(block);
                block_used = 0;
            }
        }
    }

    void finalize(byte digest[20])
    {
        const u64 bits = total * 8;
        byte pad = 0x80;
        update(&pad, 1);
        byte zero = 0;
        while (block_used != 56)
        {
            update(&zero, 1);
        }
        byte length_bytes[8];
        for (usize i = 0; i < 8; ++i)
        {
            length_bytes[i] = static_cast<byte>(bits >> (56 - 8 * i));
        }
        update(length_bytes, 8);
        for (usize i = 0; i < 5; ++i)
        {
            digest[i * 4] = static_cast<byte>(state[i] >> 24);
            digest[i * 4 + 1] = static_cast<byte>(state[i] >> 16);
            digest[i * 4 + 2] = static_cast<byte>(state[i] >> 8);
            digest[i * 4 + 3] = static_cast<byte>(state[i]);
        }
    }
};

std::string base64_encode(const byte* data, usize size)
{
    static const char* kTable =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((size + 2) / 3 * 4);
    for (usize i = 0; i < size; i += 3)
    {
        const u32 a = data[i];
        const u32 b = i + 1 < size ? data[i + 1] : 0;
        const u32 c = i + 2 < size ? data[i + 2] : 0;
        const u32 triple = (a << 16) | (b << 8) | c;
        out += kTable[(triple >> 18) & 63];
        out += kTable[(triple >> 12) & 63];
        out += i + 1 < size ? kTable[(triple >> 6) & 63] : '=';
        out += i + 2 < size ? kTable[triple & 63] : '=';
    }
    return out;
}

std::string compute_accept_key(const std::string& key)
{
    Sha1 sha1;
    sha1.update(reinterpret_cast<const byte*>(key.data()), key.size());
    sha1.update(reinterpret_cast<const byte*>(kWebSocketGuid), std::strlen(kWebSocketGuid));
    byte digest[20];
    sha1.finalize(digest);
    return base64_encode(digest, 20);
}

} // namespace

std::string websocket_accept_key(const std::string& key)
{
    return compute_accept_key(key);
}

namespace {

std::string generate_client_key()
{
    // Non-cryptographic but unique per process: time seed + counter.
    static u64 counter = 0;
    const u64 seed = static_cast<u64>(std::time(nullptr)) ^ (++counter * 2654435761u);
    byte raw[16];
    for (usize i = 0; i < 16; ++i)
    {
        raw[i] = static_cast<byte>((seed >> (i % 8 * 8)) ^ (counter * (i + 7)));
    }
    return base64_encode(raw, 16);
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
    // RFC 6455 headers use the generic HTTP grammar: one optional space
    // between the colon and the value must be trimmed before hashing.
    while (value_start < value_end && (request[value_start] == ' ' || request[value_start] == '\t'))
    {
        ++value_start;
    }
    return request.substr(value_start, value_end - value_start);
}

} // namespace

WebSocketAdapter::WebSocketAdapter(const WebSocketConfig& config)
    : config_(config)
{
}

void WebSocketAdapter::configure(const WebSocketConfig& config)
{
    config_ = config;
}

const WebSocketConfig& WebSocketAdapter::config() const
{
    return config_;
}

TransportKind WebSocketAdapter::kind() const
{
    return TransportKind::websocket;
}

ConnectionState WebSocketAdapter::state() const
{
    return tcp_.state();
}

bool WebSocketAdapter::read_exact(byte* out, usize size)
{
    usize received = 0;
    while (received < size)
    {
        if (!inbound_.empty())
        {
            const usize take = inbound_.size() < size - received ? inbound_.size() : size - received;
            std::memcpy(out + received, inbound_.data(), take);
            inbound_.erase(0, take);
            received += take;
            continue;
        }

        const TransportReceiveResult chunk = tcp_.receive_raw(size - received);
        if (!chunk.ok() || chunk.packet.empty())
        {
            return false;
        }
        inbound_.assign(
            reinterpret_cast<const char*>(chunk.packet.data().data()),
            chunk.packet.size()
        );
    }
    return true;
}

bool WebSocketAdapter::read_until_double_crlf(std::string& out)
{
    out.clear();
    while (out.size() < kMaxHandshakeBytes)
    {
        if (inbound_.empty())
        {
            const TransportReceiveResult chunk = tcp_.receive_raw(4096);
            if (!chunk.ok() || chunk.packet.empty())
            {
                return false;
            }
            inbound_.assign(
                reinterpret_cast<const char*>(chunk.packet.data().data()),
                chunk.packet.size()
            );
        }

        out += inbound_;
        inbound_.clear();

        const usize header_end = out.find("\r\n\r\n");
        if (header_end != std::string::npos)
        {
            inbound_ = out.substr(header_end + 4);
            out.resize(header_end);
            return true;
        }
    }
    return false;
}

bool WebSocketAdapter::read_frame_header(u8& fin, u8& opcode, bool& masked, u64& payload_length, byte mask_key[4])
{
    byte header[2];
    if (!read_exact(header, 2))
    {
        return false;
    }
    fin = (header[0] & 0x80) != 0 ? 1 : 0;
    opcode = header[0] & 0x0F;
    masked = (header[1] & 0x80) != 0;
    u64 length = header[1] & 0x7F;
    if (length == 126)
    {
        byte extended[2];
        if (!read_exact(extended, 2))
        {
            return false;
        }
        length = static_cast<u64>(extended[0]) << 8 | extended[1];
    }
    else if (length == 127)
    {
        byte extended[8];
        if (!read_exact(extended, 8))
        {
            return false;
        }
        length = 0;
        for (usize i = 0; i < 8; ++i)
        {
            length = (length << 8) | extended[i];
        }
    }
    if (length > config_.max_message_bytes)
    {
        return false;
    }
    payload_length = length;
    if (masked && !read_exact(mask_key, 4))
    {
        return false;
    }
    return true;
}

TransportResult WebSocketAdapter::send_frame(u8 opcode, const byte* payload, usize size)
{
    Buffer frame;
    frame.write(static_cast<byte>(0x80 | opcode));

    const bool masked = client_side_;
    if (size < 126)
    {
        frame.write(static_cast<byte>((masked ? 0x80 : 0x00) | size));
    }
    else if (size < 65536)
    {
        frame.write(static_cast<byte>((masked ? 0x80 : 0x00) | 126));
        frame.write(static_cast<byte>(size >> 8));
        frame.write(static_cast<byte>(size & 0xFF));
    }
    else
    {
        frame.write(static_cast<byte>((masked ? 0x80 : 0x00) | 127));
        for (usize i = 0; i < 8; ++i)
        {
            frame.write(static_cast<byte>(size >> (56 - 8 * i)));
        }
    }

    if (masked)
    {
        static u32 mask_seed = 0x9E3779B9u;
        mask_seed = mask_seed * 1664525u + 1013904223u;
        byte mask_key[4];
        for (usize i = 0; i < 4; ++i)
        {
            mask_key[i] = static_cast<byte>(mask_seed >> (8 * i));
        }
        frame.append(mask_key, 4);
        for (usize i = 0; i < size; ++i)
        {
            frame.write(payload[i] ^ mask_key[i % 4]);
        }
    }
    else if (size > 0)
    {
        frame.append(payload, size);
    }

    return tcp_.send_raw(frame.data().data(), frame.size());
}

TransportResult WebSocketAdapter::perform_client_handshake()
{
    const std::string key = generate_client_key();
    const TransportEndpoint endpoint = tcp_.last_endpoint();

    std::string request = "GET " + path_ + " HTTP/1.1\r\n";
    request += "Host: " + endpoint.host + "\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: " + key + "\r\n";
    request += "Sec-WebSocket-Version: 13\r\n\r\n";

    const TransportResult sent = tcp_.send_raw(
        reinterpret_cast<const byte*>(request.data()),
        request.size()
    );
    if (!sent.ok())
    {
        return sent;
    }

    std::string response;
    if (!read_until_double_crlf(response))
    {
        return {TransportStatusCode::connect_failed, "WebSocket handshake response missing.", 0};
    }
    if (response.find("101") == std::string::npos)
    {
        return {TransportStatusCode::connect_failed, "WebSocket handshake rejected by peer.", 0};
    }
    if (header_value(response, "Sec-WebSocket-Accept") != compute_accept_key(key))
    {
        return {TransportStatusCode::connect_failed, "WebSocket accept key mismatch.", 0};
    }

    handshake_done_ = true;
    return {TransportStatusCode::ok, "WebSocket handshake complete.", 0};
}

TransportResult WebSocketAdapter::send_handshake_response(const std::string& request)
{
    if (request.find("Sec-WebSocket-Version: 13") == std::string::npos
        || header_value(request, "Upgrade").find("websocket") == std::string::npos)
    {
        return {TransportStatusCode::connect_failed, "Unsupported WebSocket upgrade request.", 0};
    }

    const std::string key = header_value(request, "Sec-WebSocket-Key");
    if (key.empty())
    {
        return {TransportStatusCode::connect_failed, "WebSocket request missing Sec-WebSocket-Key.", 0};
    }

    std::string response = "HTTP/1.1 101 Switching Protocols\r\n";
    response += "Upgrade: websocket\r\n";
    response += "Connection: Upgrade\r\n";
    response += "Sec-WebSocket-Accept: " + compute_accept_key(key) + "\r\n\r\n";

    const TransportResult sent = tcp_.send_raw(
        reinterpret_cast<const byte*>(response.data()),
        response.size()
    );
    if (!sent.ok())
    {
        return sent;
    }

    handshake_done_ = true;
    return {TransportStatusCode::ok, "WebSocket handshake accepted.", 0};
}

TransportResult WebSocketAdapter::connect(const TransportEndpoint& endpoint)
{
    if (!is_valid_path(endpoint.path))
    {
        return {TransportStatusCode::invalid_endpoint, "Invalid WebSocket path.", 0};
    }

    path_ = endpoint.path.empty() ? "/" : endpoint.path;
    client_side_ = true;
    handshake_done_ = false;

    const TransportResult connected = tcp_.connect(endpoint);
    if (!connected.ok())
    {
        return connected;
    }

    const TransportResult handshake = perform_client_handshake();
    if (!handshake.ok())
    {
        (void)tcp_.close();
        return handshake;
    }
    return {TransportStatusCode::ok, "WebSocket connection established.", 0};
}

TransportResult WebSocketAdapter::listen(const TransportEndpoint& endpoint)
{
    if (!is_valid_path(endpoint.path))
    {
        return {TransportStatusCode::invalid_endpoint, "Invalid WebSocket path.", 0};
    }

    path_ = endpoint.path.empty() ? "/" : endpoint.path;
    client_side_ = false;
    handshake_done_ = false;
    return tcp_.listen(endpoint);
}

TransportResult WebSocketAdapter::accept_connection()
{
    const TransportResult accepted = tcp_.accept_connection();
    if (!accepted.ok())
    {
        return accepted;
    }

    std::string request;
    if (!read_until_double_crlf(request))
    {
        (void)tcp_.close();
        return {TransportStatusCode::connect_failed, "WebSocket handshake request missing.", 0};
    }

    const TransportResult handshake = send_handshake_response(request);
    if (!handshake.ok())
    {
        (void)tcp_.close();
        return handshake;
    }
    return {TransportStatusCode::ok, "WebSocket client accepted.", 0};
}

TransportResult WebSocketAdapter::close()
{
    if (handshake_done_ && tcp_.state() == ConnectionState::connected)
    {
        (void)send_frame(0x8, nullptr, 0); // best-effort close frame
    }
    handshake_done_ = false;
    return tcp_.close();
}

TransportResult WebSocketAdapter::send(const Buffer& packet)
{
    if (!handshake_done_ || tcp_.state() != ConnectionState::connected)
    {
        return {TransportStatusCode::not_connected, "WebSocket is not connected.", 0};
    }

    const TransportResult frame_result = send_frame(0x2, packet.data().data(), packet.size());
    if (!frame_result.ok())
    {
        return frame_result;
    }
    return {TransportStatusCode::ok, "WebSocket message sent.", packet.size()};
}

TransportReceiveResult WebSocketAdapter::receive()
{
    if (!handshake_done_ || tcp_.state() != ConnectionState::connected)
    {
        return {TransportStatusCode::not_connected, "WebSocket is not connected.", {}};
    }

    while (true)
    {
        u8 fin = 0;
        u8 opcode = 0;
        bool masked = false;
        u64 payload_length = 0;
        byte mask_key[4] = {0, 0, 0, 0};
        if (!read_frame_header(fin, opcode, masked, payload_length, mask_key))
        {
            return {TransportStatusCode::receive_failed, "WebSocket frame read failed.", {}};
        }

        // Always consume the payload first so a rejected frame cannot
        // desynchronize the frame stream.
        std::string payload(payload_length, '\0');
        if (payload_length > 0 && !read_exact(
                reinterpret_cast<byte*>(&payload[0]),
                static_cast<usize>(payload_length)))
        {
            return {TransportStatusCode::receive_failed, "WebSocket payload truncated.", {}};
        }

        // Servers must receive masked frames from clients.
        if (!client_side_ && !masked)
        {
            return {TransportStatusCode::receive_failed, "Unmasked client frame (protocol error).", {}};
        }

        if (masked)
        {
            for (usize i = 0; i < payload.size(); ++i)
            {
                payload[i] = static_cast<char>(static_cast<byte>(payload[i]) ^ mask_key[i % 4]);
            }
        }

        if (opcode == 0x8) // close
        {
            (void)send_frame(0x8, reinterpret_cast<const byte*>(payload.data()), payload.size());
            (void)tcp_.close();
            handshake_done_ = false;
            return {TransportStatusCode::receive_failed, "WebSocket closed by peer.", {}};
        }

        if (opcode == 0x9) // ping → pong
        {
            (void)send_frame(0xA, reinterpret_cast<const byte*>(payload.data()), payload.size());
            continue;
        }

        if (opcode == 0xA) // pong
        {
            continue;
        }

        if (opcode == 0x0) // continuation
        {
            if (!fragmented_ || message_.size() + payload.size() > config_.max_message_bytes)
            {
                return {TransportStatusCode::receive_failed, "Unexpected continuation frame.", {}};
            }
            message_ += payload;
            if (fin == 0)
            {
                continue;
            }
            Buffer packet;
            packet.append(reinterpret_cast<const byte*>(message_.data()), message_.size());
            message_.clear();
            fragmented_ = false;
            return {TransportStatusCode::ok, "WebSocket message received.", packet};
        }

        if (opcode == 0x1 || opcode == 0x2) // text or binary
        {
            if (fin == 0)
            {
                fragmented_ = true;
                fragment_opcode_ = opcode;
                message_ = payload;
                continue;
            }
            Buffer packet;
            if (!payload.empty())
            {
                packet.append(reinterpret_cast<const byte*>(payload.data()), payload.size());
            }
            return {TransportStatusCode::ok, "WebSocket message received.", packet};
        }

        return {TransportStatusCode::receive_failed, "Unsupported WebSocket opcode.", {}};
    }
}

u16 WebSocketAdapter::local_port() const
{
    return tcp_.local_port();
}

const std::string& WebSocketAdapter::path() const
{
    return path_;
}

bool WebSocketAdapter::is_valid_path(const std::string& path)
{
    return path.empty() || path[0] == '/';
}

}
