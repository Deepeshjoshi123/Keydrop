#include "keydrop/transport/tcp_adapter.hpp"

#include <cerrno>
#include <cstring>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace keydrop {

namespace {

constexpr int kBacklog = 1;

TransportResult fail_result(
    TransportStatusCode code,
    const std::string& message
)
{
    return {code, message, 0};
}

std::string errno_message(const char* prefix)
{
    return std::string(prefix) + ": " + std::strerror(errno);
}

bool write_all(int socket_fd, const byte* data, usize size)
{
    usize sent = 0;
    while (sent < size)
    {
        const ssize_t written = ::send(
            socket_fd,
            data + sent,
            size - sent,
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

bool read_all(int socket_fd, byte* data, usize size)
{
    usize received = 0;
    while (received < size)
    {
        const ssize_t read_count = ::recv(
            socket_fd,
            data + received,
            size - received,
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

bool fill_address(
    const TransportEndpoint& endpoint,
    sockaddr_in& out_address
)
{
    std::memset(&out_address, 0, sizeof(out_address));
    out_address.sin_family = AF_INET;
    out_address.sin_port = htons(endpoint.port);
    return ::inet_pton(AF_INET, endpoint.host.c_str(), &out_address.sin_addr) == 1;
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

    sockaddr_in address;
    if (!fill_address(endpoint, address))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::invalid_endpoint, "TCP host must be an IPv4 address.");
    }

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::connect_failed, errno_message("socket failed"));
    }

    state_ = ConnectionState::connecting;
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        ::close(fd);
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::connect_failed, errno_message("connect failed"));
    }

    socket_fd_ = fd;
    peer_fd_ = -1;
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

    sockaddr_in address;
    if (!fill_address(endpoint, address))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::invalid_endpoint, "TCP host must be an IPv4 address.");
    }

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::listen_failed, errno_message("socket failed"));
    }

    int enabled = 1;
    (void)::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        ::close(fd);
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::bind_failed, errno_message("bind failed"));
    }

    if (::listen(fd, kBacklog) != 0)
    {
        ::close(fd);
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::listen_failed, errno_message("listen failed"));
    }

    sockaddr_in bound_address;
    socklen_t bound_size = sizeof(bound_address);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&bound_address), &bound_size) == 0)
    {
        local_port_ = ntohs(bound_address.sin_port);
    }
    else
    {
        local_port_ = endpoint.port;
    }

    socket_fd_ = fd;
    peer_fd_ = -1;
    state_ = ConnectionState::listening;
    return {TransportStatusCode::ok, "TCP listener started.", 0};
}

TransportResult TcpAdapter::accept_connection()
{
    if (state_ != ConnectionState::listening || socket_fd_ < 0)
    {
        return fail_result(TransportStatusCode::not_connected, "TCP adapter is not listening.");
    }

    const int accepted_fd = ::accept(socket_fd_, nullptr, nullptr);
    if (accepted_fd < 0)
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::connect_failed, errno_message("accept failed"));
    }

    close_socket(peer_fd_);
    peer_fd_ = accepted_fd;
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
    const int fd = active_socket();
    if (fd < 0 || state_ != ConnectionState::connected)
    {
        return fail_result(TransportStatusCode::not_connected, "TCP adapter is not connected.");
    }

    if (packet.size() > 0xFFFFFFFFu)
    {
        return fail_result(TransportStatusCode::send_failed, "TCP packet is too large.");
    }

    byte header[4] = {};
    write_u32_le(header, static_cast<u32>(packet.size()));
    if (!write_all(fd, header, sizeof(header)))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::send_failed, errno_message("send header failed"));
    }

    if (!packet.empty() && !write_all(fd, packet.data().data(), packet.size()))
    {
        state_ = ConnectionState::failed;
        return fail_result(TransportStatusCode::send_failed, errno_message("send packet failed"));
    }

    return {TransportStatusCode::ok, "TCP packet sent.", packet.size()};
}

TransportReceiveResult TcpAdapter::receive()
{
    const int fd = active_socket();
    if (fd < 0 || state_ != ConnectionState::connected)
    {
        return {TransportStatusCode::not_connected, "TCP adapter is not connected.", {}};
    }

    byte header[4] = {};
    if (!read_all(fd, header, sizeof(header)))
    {
        state_ = ConnectionState::failed;
        return {TransportStatusCode::receive_failed, errno_message("receive header failed"), {}};
    }

    const u32 packet_size = read_u32_le(header);
    Buffer packet;
    if (packet_size != 0)
    {
        packet.resize(packet_size);
        if (!read_all(fd, packet.mutable_bytes(), packet_size))
        {
            state_ = ConnectionState::failed;
            return {TransportStatusCode::receive_failed, errno_message("receive packet failed"), {}};
        }
    }

    return {TransportStatusCode::ok, "TCP packet received.", packet};
}

u16 TcpAdapter::local_port() const
{
    return local_port_;
}

int TcpAdapter::active_socket() const
{
    if (peer_fd_ >= 0)
    {
        return peer_fd_;
    }
    return socket_fd_;
}

void TcpAdapter::close_socket(int& socket_fd)
{
    if (socket_fd >= 0)
    {
        ::close(socket_fd);
        socket_fd = -1;
    }
}

}
