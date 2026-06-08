#pragma once

#include <string>

#include "keydrop/core/buffer.hpp"
#include "keydrop/core/types.hpp"

namespace keydrop {

enum class TransportKind {
    tcp,
    websocket
};

enum class ConnectionState {
    disconnected,
    connecting,
    listening,
    connected,
    closing,
    failed
};

enum class TransportStatusCode {
    ok,
    invalid_endpoint,
    not_connected,
    already_connected,
    send_failed,
    receive_failed,
    bind_failed,
    listen_failed,
    connect_failed,
    unsupported
};

struct TransportEndpoint {
    std::string host = "127.0.0.1";
    u16 port = 0;
    std::string path;
};

struct TransportResult {
    TransportStatusCode code = TransportStatusCode::ok;
    std::string message;
    usize bytes_transferred = 0;

    bool ok() const
    {
        return code == TransportStatusCode::ok;
    }
};

struct TransportReceiveResult {
    TransportStatusCode code = TransportStatusCode::ok;
    std::string message;
    Buffer packet;

    bool ok() const
    {
        return code == TransportStatusCode::ok;
    }
};

class Transport {
public:
    virtual ~Transport();

    virtual TransportKind kind() const = 0;
    virtual ConnectionState state() const = 0;

    virtual TransportResult connect(const TransportEndpoint& endpoint) = 0;
    virtual TransportResult listen(const TransportEndpoint& endpoint) = 0;
    virtual TransportResult close() = 0;

    virtual TransportResult send(const Buffer& packet) = 0;
    virtual TransportReceiveResult receive() = 0;
};

const char* transport_kind_to_string(TransportKind kind);
const char* connection_state_to_string(ConnectionState state);
const char* transport_status_to_string(TransportStatusCode code);

bool is_valid_endpoint(const TransportEndpoint& endpoint);

}
