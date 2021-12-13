#include <iostream>

#include "spinet.h"
#include "util.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: PROGRAM_NAME ${address} ${port}" << std::endl;
        return EXIT_FAILURE;
    }
    auto port = expectInt(argv[2], "port is an invalid number.");
    if (port < 0 || port > 65535) {
        std::cerr << "port should be between 0 and 65535." << std::endl;
        return EXIT_FAILURE;
    }
    auto res = spinet::Address::resolve(argv[1], port);
    if (res.index() == 1) {
        std::cerr << std::get<1>(res) << std::endl;
        return EXIT_FAILURE;
    }
    auto addresses = std::get<0>(res);
    for (auto& address : addresses) {
        std::cerr << address.to_string() << std::endl;
    }
    return EXIT_SUCCESS;
}