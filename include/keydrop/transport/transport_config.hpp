#pragma once

#include <memory>

#include "keydrop/transport/transport.hpp"

namespace keydrop {

struct TransportConfig {
    TransportKind kind = TransportKind::tcp;
    TransportEndpoint endpoint;
    bool server_mode = false;
};

struct TransportCreateResult {
    TransportStatusCode code = TransportStatusCode::ok;
    std::string message;
    std::unique_ptr<Transport> transport;

    bool ok() const
    {
        return code == TransportStatusCode::ok && transport != nullptr;
    }
};

TransportCreateResult create_transport(const TransportConfig& config);

}
