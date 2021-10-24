#include "netdb.h"

#include "fmt/format.h"

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
            return Address { type, std::string { buf }, ntohs(sockaddr.sin_port) };
        } else {
            return fmt::format("sockaddr is invalid, reason:{}", std::strerror(errno));
        }
    }
}

std::variant<Address, std::string> Address::parse(const std::string& ip, uint16_t port) {
    return parse(ip.c_str(), port);
}

std::variant<std::vector<Address>, std::string> Address::resolve(const char* domain, uint16_t port) {
    ::addrinfo hints { 0 };
    hints.ai_family = PF_UNSPEC;
    ::addrinfo* res = nullptr;
    {
        int ec = ::getaddrinfo(domain, std::to_string(port).c_str(), &hints, &res);
        if (ec != 0) {
            return ::gai_strerror(ec);
        }
    }
    std::vector<Address> addresses {};
    for (::addrinfo* iter = res; iter != nullptr; iter = iter->ai_next) {
        char buf[std::max(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)] = { 0 };
        if (iter->ai_family == AF_INET) {
            ::inet_ntop(AF_INET, iter->ai_addr->sa_data, buf, INET_ADDRSTRLEN);
            Address address { IPV4, std::string { buf }, ntohs(((sockaddr_in*)iter->ai_addr)->sin_port) };
            addresses.push_back(address);
        } else {
            ::inet_ntop(AF_INET6, iter->ai_addr->sa_data, buf, INET6_ADDRSTRLEN);
            Address address { IPV6, std::string { buf }, ((sockaddr_in6*)iter->ai_addr)->sin6_port };
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
    return fmt::format("{}:{}", address_, port_);
}

Address::Address(Type type, std::string address, uint16_t port)
: type_ { type }
, address_ { address }
, port_ { port } {
}