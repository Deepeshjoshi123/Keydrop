#include "keydrop/transport/transport.hpp"

namespace keydrop {

Transport::~Transport() = default;

const char* transport_kind_to_string(TransportKind kind)
{
    switch (kind)
    {
    case TransportKind::tcp: return "tcp";
    case TransportKind::websocket: return "websocket";
    }

    return "unknown";
}

const char* connection_state_to_string(ConnectionState state)
{
    switch (state)
    {
    case ConnectionState::disconnected: return "disconnected";
    case ConnectionState::connecting: return "connecting";
    case ConnectionState::listening: return "listening";
    case ConnectionState::connected: return "connected";
    case ConnectionState::closing: return "closing";
    case ConnectionState::failed: return "failed";
    }

    return "unknown";
}

const char* transport_status_to_string(TransportStatusCode code)
{
    switch (code)
    {
    case TransportStatusCode::ok: return "ok";
    case TransportStatusCode::invalid_endpoint: return "invalid_endpoint";
    case TransportStatusCode::not_connected: return "not_connected";
    case TransportStatusCode::already_connected: return "already_connected";
    case TransportStatusCode::send_failed: return "send_failed";
    case TransportStatusCode::receive_failed: return "receive_failed";
    case TransportStatusCode::bind_failed: return "bind_failed";
    case TransportStatusCode::listen_failed: return "listen_failed";
    case TransportStatusCode::connect_failed: return "connect_failed";
    case TransportStatusCode::unsupported: return "unsupported";
    }

    return "unknown";
}

bool is_valid_endpoint(const TransportEndpoint& endpoint)
{
    return !endpoint.host.empty() && endpoint.port != 0;
}

}
