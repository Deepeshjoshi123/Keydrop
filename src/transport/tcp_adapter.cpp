#include "keydrop/transport/tcp_adapter.hpp"

#include "keydrop/platform/socket.hpp"

#include <climits>
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
#include <unistd.h>

#endif

namespace keydrop {

namespace {

constexpr int kBacklog = 1;

#ifdef _WIN32
using NativeSocket = SOCKET;
using SocketLength = int;
using SocketIoResult = int;

NativeSocket to_native_socket(std::uintptr_t socket)
{
    return static_cast<NativeSocket>(socket);
}

std::uintptr_t from_native_socket(NativeSocket socket)
{
    return static_cast<std::uintptr_t>(socket);
}

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
using SocketLength = socklen_t;
using SocketIoResult = ssize_t;

NativeSocket to_native_socket(std::uintptr_t socket)
{
    return static_cast<NativeSocket>(socket);
}

std::uintptr_t from_native_socket(NativeSocket socket)
{
    return static_cast<std::uintptr_t>(socket);
}

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

bool write_all(NativeSocket socket, const byte* data, usize size)
{
    usize sent = 0;
    while (sent < size)
    {
        const usize remaining = size - sent;
        const int chunk_size = static_cast<int>(remaining > static_cast<usize>(INT_MAX)
            ? INT_MAX
            : remaining);
        const SocketIoResult written = ::send(
            socket,
            reinterpret_cast<const char*>(data + sent),
            chunk_size,
            0
        );
        if (written <= 0)
        {
            return false;
        }
        sent += static_cast<usize>(written);
    }

    return true;
}

bool read_all(NativeSocket socket, byte* data, usize size)
{
    usize received = 0;
    while (received < size)
    {
        const usize remaining = size - received;
        const int chunk_size = static_cast<int>(remaining > static_cast<usize>(INT_MAX)
            ? INT_MAX
            : remaining);
        const SocketIoResult read_count = ::recv(
            socket,
            reinterpret_cast<char*>(data + received),
            chunk_size,
            0
        );
        if (read_count <= 0)
        {
            return false;
        }
        received += static_cast<usize>(read_count);
    }

    return true;
}

void write_u32_le(byte* out, u32 value)
{
    out[0] = static_cast<byte>(value & 0xFF);
    out[1] = static_cast<byte>((value >> 8) & 0xFF);
    out[2] = static_cast<byte>((value >> 16) & 0xFF);
    out[3] = static_cast<byte>((value >> 24) & 0xFF);
}

u32 read_u32_le(const byte* data)
{
    return static_cast<u32>(data[0])
        | (static_cast<u32>(data[1]) << 8)
        | (static_cast<u32>(data[2]) << 16)
        | (static_cast<u32>(data[3]) << 24);
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

TcpAdapter::~TcpAdapter()
{
    (void)close();
}

TransportKind TcpAdapter::kind() const
{
    return TransportKind::tcp;
}

ConnectionState TcpAdapter::state() const
{
    return state_;
}

TransportResult TcpAdapter::connect(const TransportEndpoint& endpoint)
{
    if (!is_valid_endpoint(endpoint))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::invalid_endpoint, "Invalid TCP endpoint.");
    }

    if (state_ == ConnectionState::connected)
    {
        return fail_result(TransportStatusCode::already_connected, "TCP adapter is already connected.");
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
        return fail_result(TransportStatusCode::invalid_endpoint, "TCP host must be an IPv4 address.");
    }

    const NativeSocket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (is_invalid_socket(socket))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::connect_failed, socket_error_message("socket failed"));
    }

    state_ = ConnectionState::connecting;
    if (::connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        close_native_socket(socket);
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::connect_failed, socket_error_message("connect failed"));
    }

    socket_fd_ = from_native_socket(socket);
    peer_fd_ = platform::kInvalidSocket;
    local_port_ = endpoint.port;
    state_ = ConnectionState::connected;
    return {TransportStatusCode::ok, "TCP connection established.", 0};
}

TransportResult TcpAdapter::listen(const TransportEndpoint& endpoint)
{
    if (endpoint.host.empty())
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::invalid_endpoint, "Invalid TCP listen endpoint.");
    }

    if (state_ == ConnectionState::connected || state_ == ConnectionState::listening)
    {
        return fail_result(TransportStatusCode::already_connected, "TCP adapter is already active.");
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
        return fail_result(TransportStatusCode::invalid_endpoint, "TCP host must be an IPv4 address.");
    }

    const NativeSocket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (is_invalid_socket(socket))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::listen_failed, socket_error_message("socket failed"));
    }

    int enabled = 1;
    (void)::setsockopt(
        socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&enabled),
        sizeof(enabled)
    );

    if (::bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        close_native_socket(socket);
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::bind_failed, socket_error_message("bind failed"));
    }

    if (::listen(socket, kBacklog) != 0)
    {
        close_native_socket(socket);
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::listen_failed, socket_error_message("listen failed"));
    }

    sockaddr_in bound_address{};
    SocketLength bound_size = sizeof(bound_address);
    if (::getsockname(socket, reinterpret_cast<sockaddr*>(&bound_address), &bound_size) == 0)
    {
        local_port_ = ntohs(bound_address.sin_port);
    }
    else
    {
        local_port_ = endpoint.port;
    }

    socket_fd_ = from_native_socket(socket);
    peer_fd_ = platform::kInvalidSocket;
    state_ = ConnectionState::listening;
    return {TransportStatusCode::ok, "TCP listener started.", 0};
}

TransportResult TcpAdapter::accept_connection()
{
    if (state_ != ConnectionState::listening || socket_fd_ == platform::kInvalidSocket)
    {
        return fail_result(TransportStatusCode::not_connected, "TCP adapter is not listening.");
    }

    const NativeSocket accepted_socket = ::accept(to_native_socket(socket_fd_), nullptr, nullptr);
    if (is_invalid_socket(accepted_socket))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::connect_failed, socket_error_message("accept failed"));
    }

    close_socket(peer_fd_);
    peer_fd_ = from_native_socket(accepted_socket);
    state_ = ConnectionState::connected;
    return {TransportStatusCode::ok, "TCP client accepted.", 0};
}

TransportResult TcpAdapter::close()
{
    state_ = ConnectionState::closing;
    close_socket(peer_fd_);
    close_socket(socket_fd_);
    local_port_ = 0;
    state_ = ConnectionState::disconnected;
    return {TransportStatusCode::ok, "TCP adapter closed.", 0};
}

TransportResult TcpAdapter::send(const Buffer& packet)
{
    const std::uintptr_t socket_handle = active_socket();
    if (socket_handle == platform::kInvalidSocket || state_ != ConnectionState::connected)
    {
        return fail_result(TransportStatusCode::not_connected, "TCP adapter is not connected.");
    }

    if (packet.size() > 0xFFFFFFFFu)
    {
        return fail_result(TransportStatusCode::send_failed, "TCP packet is too large.");
    }

    const NativeSocket socket = to_native_socket(socket_handle);
    byte header[4] = {};
    write_u32_le(header, static_cast<u32>(packet.size()));
    if (!write_all(socket, header, sizeof(header)))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::send_failed, socket_error_message("send header failed"));
    }

    if (!packet.empty() && !write_all(socket, packet.data().data(), packet.size()))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::send_failed, socket_error_message("send packet failed"));
    }

    return {TransportStatusCode::ok, "TCP packet sent.", packet.size()};
}

TransportReceiveResult TcpAdapter::receive()
{
    const std::uintptr_t socket_handle = active_socket();
    if (socket_handle == platform::kInvalidSocket || state_ != ConnectionState::connected)
    {
        return {TransportStatusCode::not_connected, "TCP adapter is not connected.", {}};
    }

    const NativeSocket socket = to_native_socket(socket_handle);
    byte header[4] = {};
    if (!read_all(socket, header, sizeof(header)))
    {
        state_ = ConnectionState::failed;
        return {TransportStatusCode::receive_failed, socket_error_message("receive header failed"), {}};
    }

    const u32 packet_size = read_u32_le(header);
    Buffer packet;
    if (packet_size != 0)
    {
        packet.resize(packet_size);
        if (!read_all(socket, packet.mutable_bytes(), packet_size))
        {
            state_ = ConnectionState::failed;
            return {TransportStatusCode::receive_failed, socket_error_message("receive packet failed"), {}};
        }
    }

    return {TransportStatusCode::ok, "TCP packet received.", packet};
}

u16 TcpAdapter::local_port() const
{
    return local_port_;
}

std::uintptr_t TcpAdapter::active_socket() const
{
    return peer_fd_ != platform::kInvalidSocket ? peer_fd_ : socket_fd_;
}

void TcpAdapter::close_socket(std::uintptr_t& socket_handle)
{
    if (socket_handle != platform::kInvalidSocket)
    {
        close_native_socket(to_native_socket(socket_handle));
        socket_handle = platform::kInvalidSocket;
    }
}

} // namespace keydrop
