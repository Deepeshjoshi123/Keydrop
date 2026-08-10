#include "keydrop/transport/udp_adapter.hpp"

#include "keydrop/platform/socket.hpp"

#include <cstring>
#include <string>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#else

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#endif

namespace keydrop {

namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;

bool is_invalid_socket(NativeSocket socket)
{
    return socket == INVALID_SOCKET;
}

void close_native_socket(NativeSocket socket)
{
    (void)::closesocket(socket);
}
#else
using NativeSocket = int;

bool is_invalid_socket(NativeSocket socket)
{
    return socket < 0;
}

void close_native_socket(NativeSocket socket)
{
    (void)::close(socket);
}
#endif

TransportResult fail_result(TransportStatusCode code, const std::string& message)
{
    return {code, message, 0};
}

std::string socket_error_message(const char* prefix)
{
    return std::string(prefix) + ": " + platform::last_socket_error();
}

bool fill_address(const TransportEndpoint& endpoint, sockaddr_in& out_address)
{
    std::memset(&out_address, 0, sizeof(out_address));
    out_address.sin_family = AF_INET;
    out_address.sin_port = htons(endpoint.port);
    const unsigned long parsed_address = ::inet_addr(endpoint.host.c_str());
    if (parsed_address == INADDR_NONE)
    {
        return false;
    }
    out_address.sin_addr.s_addr = parsed_address;
    return true;
}

} // namespace

UdpAdapter::UdpAdapter(const UdpConfig& config)
    : config_(config)
{
}

UdpAdapter::~UdpAdapter()
{
    (void)close();
}

void UdpAdapter::configure(const UdpConfig& config)
{
    config_ = config;
}

const UdpConfig& UdpAdapter::config() const
{
    return config_;
}

TransportKind UdpAdapter::kind() const
{
    return TransportKind::udp;
}

ConnectionState UdpAdapter::state() const
{
    return state_;
}

TransportResult UdpAdapter::connect(const TransportEndpoint& endpoint)
{
    if (!is_valid_endpoint(endpoint))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::invalid_endpoint, "Invalid UDP endpoint.");
    }

    std::string initialization_error;
    if (!platform::ensure_socket_subsystem(initialization_error))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::connect_failed, initialization_error);
    }

    sockaddr_in address;
    if (!fill_address(endpoint, address))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::invalid_endpoint, "UDP host must be an IPv4 address.");
    }

    const NativeSocket socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (is_invalid_socket(socket))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::connect_failed, socket_error_message("socket failed"));
    }

    if (::connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        close_native_socket(socket);
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::connect_failed, socket_error_message("udp connect failed"));
    }

    socket_fd_ = static_cast<std::uintptr_t>(socket);
    state_ = ConnectionState::connected;
    return {TransportStatusCode::ok, "UDP peer set.", 0};
}

TransportResult UdpAdapter::listen(const TransportEndpoint& endpoint)
{
    if (endpoint.host.empty())
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::invalid_endpoint, "Invalid UDP listen endpoint.");
    }

    std::string initialization_error;
    if (!platform::ensure_socket_subsystem(initialization_error))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::listen_failed, initialization_error);
    }

    sockaddr_in address;
    if (!fill_address(endpoint, address))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::invalid_endpoint, "UDP host must be an IPv4 address.");
    }

    const NativeSocket socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (is_invalid_socket(socket))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::listen_failed, socket_error_message("socket failed"));
    }

    if (::bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        close_native_socket(socket);
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::bind_failed, socket_error_message("bind failed"));
    }

    sockaddr_in bound_address {};
#ifdef _WIN32
    int bound_size = sizeof(bound_address);
#else
    socklen_t bound_size = sizeof(bound_address);
#endif
    if (::getsockname(socket, reinterpret_cast<sockaddr*>(&bound_address), &bound_size) == 0)
    {
        local_port_ = ntohs(bound_address.sin_port);
    }
    else
    {
        local_port_ = endpoint.port;
    }

    if (config_.receive_timeout_ms != 0)
    {
#ifdef _WIN32
        const DWORD ms = static_cast<DWORD>(config_.receive_timeout_ms);
        (void)::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&ms), sizeof(ms));
#else
        timeval timeout;
        timeout.tv_sec = static_cast<time_t>(config_.receive_timeout_ms / 1000);
        timeout.tv_usec = static_cast<suseconds_t>((config_.receive_timeout_ms % 1000) * 1000);
        (void)::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
    }

    socket_fd_ = static_cast<std::uintptr_t>(socket);
    state_ = ConnectionState::listening;
    return {TransportStatusCode::ok, "UDP listener started.", 0};
}

TransportResult UdpAdapter::close()
{
    if (socket_fd_ != static_cast<std::uintptr_t>(-1))
    {
        close_native_socket(static_cast<NativeSocket>(socket_fd_));
        socket_fd_ = static_cast<std::uintptr_t>(-1);
    }
    local_port_ = 0;
    state_ = ConnectionState::disconnected;
    return {TransportStatusCode::ok, "UDP adapter closed.", 0};
}

TransportResult UdpAdapter::send(const Buffer& packet)
{
    if (state_ != ConnectionState::connected && state_ != ConnectionState::listening)
    {
        return fail_result(TransportStatusCode::not_connected, "UDP adapter is not bound or connected.");
    }

    const usize framing = 6;
    if (packet.size() + framing > config_.max_datagram_bytes)
    {
        return fail_result(TransportStatusCode::send_failed, "UDP packet exceeds max_datagram_bytes (MTU bound).");
    }

    Buffer datagram;
    datagram.reserve(packet.size() + framing);
    const u32 seq = next_sequence_++;
    if (config_.drop_sequence != 0 && seq == config_.drop_sequence)
    {
        stats_.datagrams_sent += 1; // sequence consumed; datagram dropped
        return {TransportStatusCode::ok, "UDP datagram dropped (loss hook).", packet.size()};
    }
    datagram.write(static_cast<byte>(seq & 0xFF));
    datagram.write(static_cast<byte>((seq >> 8) & 0xFF));
    datagram.write(static_cast<byte>((seq >> 16) & 0xFF));
    datagram.write(static_cast<byte>((seq >> 24) & 0xFF));
    datagram.write(static_cast<byte>(packet.size() & 0xFF));
    datagram.write(static_cast<byte>((packet.size() >> 8) & 0xFF));
    datagram.append(packet);

    int sent = -1;
    if (state_ == ConnectionState::listening)
    {
        // Bound but not connected: reply to the last received source.
        if (!peer_set_)
        {
            return fail_result(TransportStatusCode::send_failed, "UDP adapter has no peer to send to yet.");
        }
        sockaddr_in peer;
        std::memset(&peer, 0, sizeof(peer));
        peer.sin_family = AF_INET;
        peer.sin_port = htons(peer_port_);
        peer.sin_addr.s_addr = peer_addr_;
        sent = ::sendto(
            static_cast<NativeSocket>(socket_fd_),
            reinterpret_cast<const char*>(datagram.data().data()),
            static_cast<int>(datagram.size()),
            0,
            reinterpret_cast<sockaddr*>(&peer),
            sizeof(peer)
        );
    }
    else
    {
        sent = ::send(
            static_cast<NativeSocket>(socket_fd_),
            reinterpret_cast<const char*>(datagram.data().data()),
            static_cast<int>(datagram.size()),
            0
        );
    }
    if (sent < 0)
    {
        return fail_result(TransportStatusCode::send_failed, socket_error_message("udp send failed"));
    }

    stats_.datagrams_sent += 1;
    return {TransportStatusCode::ok, "UDP datagram sent.", packet.size()};
}

TransportReceiveResult UdpAdapter::receive()
{
    if (socket_fd_ == static_cast<std::uintptr_t>(-1))
    {
        return {TransportStatusCode::not_connected, "UDP adapter has no socket.", {}};
    }

    Buffer datagram;
    datagram.resize(config_.max_datagram_bytes);
    sockaddr_in source;
    std::memset(&source, 0, sizeof(source));
#ifdef _WIN32
    int source_size = sizeof(source);
#else
    socklen_t source_size = sizeof(source);
#endif
    const int received = ::recvfrom(
        static_cast<NativeSocket>(socket_fd_),
        reinterpret_cast<char*>(datagram.mutable_bytes()),
        static_cast<int>(config_.max_datagram_bytes),
        0,
        reinterpret_cast<sockaddr*>(&source),
        &source_size
    );
    if (received < 0)
    {
        return {TransportStatusCode::receive_failed, socket_error_message("udp receive failed"), {}};
    }
    datagram.resize(static_cast<usize>(received));
    peer_addr_ = source.sin_addr.s_addr;
    peer_port_ = ntohs(source.sin_port);
    peer_set_ = true;

    if (datagram.size() < 6)
    {
        return {TransportStatusCode::receive_failed, "UDP datagram smaller than framing.", {}};
    }

    const std::vector<byte>& bytes = datagram.data();
    const u32 seq = static_cast<u32>(bytes[0])
        | (static_cast<u32>(bytes[1]) << 8)
        | (static_cast<u32>(bytes[2]) << 16)
        | (static_cast<u32>(bytes[3]) << 24);
    const usize length = static_cast<usize>(bytes[4]) | (static_cast<usize>(bytes[5]) << 8);
    if (6 + length > datagram.size())
    {
        return {TransportStatusCode::receive_failed, "UDP datagram length exceeds datagram.", {}};
    }

    // Sequence bookkeeping: detect loss, reordering, duplicates.
    if (stats_.has_sequence)
    {
        if (seq > stats_.last_sequence)
        {
            stats_.lost += static_cast<u64>(seq - stats_.last_sequence - 1);
            stats_.last_sequence = seq;
        }
        else
        {
            stats_.reordered += 1; // duplicate or out-of-order
        }
    }
    else
    {
        stats_.has_sequence = true;
        stats_.last_sequence = seq;
    }
    stats_.datagrams_received += 1;

    Buffer packet;
    packet.append(&bytes[6], length);
    return {TransportStatusCode::ok, "UDP datagram received.", packet};
}

u16 UdpAdapter::local_port() const
{
    return local_port_;
}

const UdpStats& UdpAdapter::stats() const
{
    return stats_;
}

void UdpAdapter::reset_stats()
{
    stats_ = UdpStats {};
}

}
