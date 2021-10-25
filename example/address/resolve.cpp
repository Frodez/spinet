#include <iostream>

#include "spinet.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: PROGRAM_NAME ${address} ${port}" << std::endl;
        return EXIT_FAILURE;
    }
    char* ptr = nullptr;
    long port = strtol(argv[2], &ptr, 10);
    if (*ptr != 0) {
        std::cout << "port is an invalid number." << std::endl;
        return EXIT_FAILURE;
    }
    if (port < 0 || port > 65535) {
        std::cout << "port should be between 0 and 65535." << std::endl;
        return EXIT_FAILURE;
    }
    auto res = spinet::Address::resolve(argv[1], port);
    if (res.index() == 1) {
        std::cout << std::get<1>(res) << std::endl;
        return EXIT_FAILURE;
    }
    auto addresses = std::get<0>(res);
    for (auto& address : addresses) {
        std::cout << address.to_string() << std::endl;
    }
    return EXIT_SUCCESS;
}