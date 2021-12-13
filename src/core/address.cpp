#include <utility>

#include "netdb.h"

#include "spinet/core/address.h"
#include "util.h"

using namespace spinet;

Address::~Address() {
}

std::variant<Address, std::string> Address::parse(const char* ip, uint16_t port) {
    auto res = to_sockaddr_in(ip, port);
    if (res.index() == 1) {
        return std::get<1>(res);
    } else {
        auto& sockaddr = std::get<0>(res);
        std::size_t len {};
        Type type {};
        char buf[std::max(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)] = { 0 };
        if (sockaddr.sin_family == AF_INET) {
            len = INET_ADDRSTRLEN;
            type = IPV4;
        } else {
            len = INET6_ADDRSTRLEN;
            type = IPV6;
        }
        if (::inet_ntop(sockaddr.sin_family, &sockaddr.sin_addr, buf, len) != nullptr) {
            return Address { type, std::string { buf }, port };
        } else {
            return std::string { "sockaddr is invalid, reason:" } + std::strerror(errno);
        }
    }
}

std::variant<Address, std::string> Address::parse(const std::string& ip, uint16_t port) {
    return parse(ip.c_str(), port);
}

std::variant<std::vector<Address>, std::string> Address::resolve(const char* domain, uint16_t port) {
    ::addrinfo hints {};
    hints.ai_family = PF_UNSPEC;
    ::addrinfo* res = nullptr;
    {
        int ec = ::getaddrinfo(domain, nullptr, &hints, &res);
        if (ec != 0) {
            return ::gai_strerror(ec);
        }
    }
    std::vector<Address> addresses {};
    for (::addrinfo* iter = res; iter != nullptr; iter = iter->ai_next) {
        if (iter->ai_protocol != IPPROTO_IP) {
            continue;
        }
        char buf[std::max(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)] = { 0 };
        if (iter->ai_family == AF_INET) {
            sockaddr_in* sockaddr = (sockaddr_in*)iter->ai_addr;
            ::inet_ntop(AF_INET, &sockaddr->sin_addr, buf, INET_ADDRSTRLEN);
            Address address { IPV4, std::string { buf }, port };
            addresses.push_back(address);
        } else {
            sockaddr_in6* sockaddr = (sockaddr_in6*)iter->ai_addr;
            ::inet_ntop(AF_INET6, &sockaddr->sin6_addr, buf, INET6_ADDRSTRLEN);
            Address address { IPV6, std::string { buf }, port };
            addresses.push_back(address);
        }
    }
    ::freeaddrinfo(res);
    return addresses;
}

std::variant<std::vector<Address>, std::string> Address::resolve(const std::string& domain, uint16_t port) {
    return resolve(domain.c_str(), port);
}

Address::Type Address::type() {
    return type_;
}

std::string Address::address() {
    return address_;
}

uint16_t Address::port() {
    return port_;
}

std::string Address::to_string() {
    return address_ + ":" + std::to_string(port_);
}

Address::Address(Type type, std::string address, uint16_t port)
: type_ { type }
, address_ { std::move(address) }
, port_ { port } {
}