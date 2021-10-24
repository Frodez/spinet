#pragma once

#include <string>
#include <variant>
#include <vector>

namespace spinet {

class Address {
    public:
    enum Type { IPV4, IPV6 };

    ~Address();

    static std::variant<Address, std::string> parse(const char* ip, uint16_t port);
    static std::variant<Address, std::string> parse(const std::string& ip, uint16_t port);
    static std::variant<std::vector<Address>, std::string> resolve(const char* domain, uint16_t port);
    static std::variant<std::vector<Address>, std::string> resolve(const std::string& domain, uint16_t port);

    Type type();
    std::string address();
    uint16_t port();

    std::string to_string();

    private:
    Address(Type type, std::string address, uint16_t port);

    Type type_;
    std::string address_;
    uint16_t port_;
};

}