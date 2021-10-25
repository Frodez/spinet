#pragma once

#include <cstring>
#include <string>
#include <variant>

#include "arpa/inet.h"
#include "fcntl.h"
#include "netinet/in.h"
#include "sys/socket.h"

namespace spinet {

inline void set_nonblock(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    ::fcntl(fd, F_SETFL, flags);
}

inline void set_reuse_port(int fd) {
    int option { 1 };
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option));
}

inline std::string from_sockaddr_in(const ::sockaddr_in& address) {
    char buf[std::max(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)] = { 0 };
    ::inet_ntop(address.sin_family, &address.sin_addr, buf, address.sin_family == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN);
    return { buf };
}

inline std::variant<::sockaddr_in, std::string> to_sockaddr_in(const char* ip, uint16_t port) {
    ::sockaddr_in address { 0 };
    address.sin_port = htons(port);
    if (::inet_pton(AF_INET, ip, &address.sin_addr) == 1) {
        address.sin_family = AF_INET;
        return address;
    } else if (::inet_pton(AF_INET6, ip, &address.sin_addr) == 1) {
        address.sin_family = AF_INET6;
        return address;
    } else {
        return std::string { "parse failed, reason:" } + std::strerror(errno);
    }
}

}