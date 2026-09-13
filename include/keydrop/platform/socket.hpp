#pragma once

#include <cstdint>
#include <limits>
#include <string>

namespace keydrop::platform {

// An opaque, pointer-width socket value.  Keeping this type out of the
// transport public API prevents platform socket headers from leaking to users.
using SocketHandle = std::uintptr_t;

constexpr SocketHandle kInvalidSocket = std::numeric_limits<SocketHandle>::max();

// Initializes the process-wide socket subsystem where the operating system
// requires it (Winsock on Windows). It is safe to call more than once.
bool ensure_socket_subsystem(std::string& error_message);

// Returns the error associated with the last failed socket operation.
std::string last_socket_error();

} // namespace keydrop::platform
