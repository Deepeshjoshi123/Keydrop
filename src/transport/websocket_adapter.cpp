#include "keydrop/transport/websocket_adapter.hpp"

namespace keydrop {

TransportKind WebSocketAdapter::kind() const
{
    return TransportKind::websocket;
}

ConnectionState WebSocketAdapter::state() const
{
    return tcp_.state();
}

TransportResult WebSocketAdapter::connect(const TransportEndpoint& endpoint)
{
    if (!is_valid_path(endpoint.path))
    {
        return {TransportStatusCode::invalid_endpoint, "Invalid WebSocket path.", 0};
    }

    path_ = endpoint.path.empty() ? "/" : endpoint.path;
    return tcp_.connect(endpoint);
}

TransportResult WebSocketAdapter::listen(const TransportEndpoint& endpoint)
{
    if (!is_valid_path(endpoint.path))
    {
        return {TransportStatusCode::invalid_endpoint, "Invalid WebSocket path.", 0};
    }

    path_ = endpoint.path.empty() ? "/" : endpoint.path;
    return tcp_.listen(endpoint);
}

TransportResult WebSocketAdapter::close()
{
    return tcp_.close();
}

TransportResult WebSocketAdapter::send(const Buffer& packet)
{
    return tcp_.send(packet);
}

TransportReceiveResult WebSocketAdapter::receive()
{
    return tcp_.receive();
}

TransportResult WebSocketAdapter::accept_connection()
{
    return tcp_.accept_connection();
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
