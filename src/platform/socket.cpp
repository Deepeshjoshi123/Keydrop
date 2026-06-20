#include "keydrop/platform/socket.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#else

#include <cerrno>
#include <cstring>

#endif

namespace keydrop::platform {

bool ensure_socket_subsystem(std::string& error_message)
{
#ifdef _WIN32
    static const int initialization_result = []() {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data);
    }();

    if (initialization_result != 0)
    {
        error_message = "WSAStartup failed: " + std::to_string(initialization_result);
        return false;
    }
#else
    (void)error_message;
#endif

    return true;
}

std::string last_socket_error()
{
#ifdef _WIN32
    return "Winsock error " + std::to_string(WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

} // namespace keydrop::platform
