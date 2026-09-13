#include "keydrop/transport/transport_config.hpp"

#include "keydrop/transport/tcp_adapter.hpp"
#include "keydrop/transport/websocket_adapter.hpp"

namespace keydrop {

TransportCreateResult create_transport(const TransportConfig& config)
{
    if (config.endpoint.host.empty())
    {
        return {
            TransportStatusCode::invalid_endpoint,
            "Transport endpoint host cannot be empty.",
            nullptr
        };
    }

    switch (config.kind)
    {
    case TransportKind::tcp:
        return {
            TransportStatusCode::ok,
            "TCP transport created.",
            std::unique_ptr<Transport>(new TcpAdapter())
        };

    case TransportKind::websocket:
        return {
            TransportStatusCode::ok,
            "WebSocket transport created.",
            std::unique_ptr<Transport>(new WebSocketAdapter())
        };
    }

    return {
        TransportStatusCode::unsupported,
        "Unsupported transport kind.",
        nullptr
    };
}

}
