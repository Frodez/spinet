#include <atomic>
#include <cstring>
#include <iostream>

#include "spinet.h"
#include "util.h"

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: PROGRAM_NAME ${address} ${port} ${worker_threads} ${times} ${payload_size}" << std::endl;
        return EXIT_FAILURE;
    }
    auto port = expectInt(argv[2], "port is an invalid number.");
    if (port < 0 || port > 65535) {
        std::cerr << "port should be between 0 and 65535." << std::endl;
        return EXIT_FAILURE;
    }
    auto address = spinet::Address::parse(argv[1], port);
    if (address.index() == 1) {
        std::cerr << std::get<1>(address) << std::endl;
        return EXIT_FAILURE;
    }
    auto worker_threads = expectInt(argv[3], "worker_threads is an invalid number.");
    if (worker_threads <= 0) {
        std::cerr << "worker_threads should be greater than 0." << std::endl;
        return EXIT_FAILURE;
    }
    auto times = expectInt(argv[4], "times is an invalid number.");
    if (times <= 0) {
        std::cerr << "times should be greater than 0." << std::endl;
        return EXIT_FAILURE;
    }
    auto payload_size = expectInt(argv[5], "payload_size is an invalid number.");
    if (payload_size <= 0) {
        std::cerr << "payload_size should be greater than 0." << std::endl;
        return EXIT_FAILURE;
    }
    spinet::Client client {};
    spinet::Client::Settings settings { .workers = (uint16_t)worker_threads, .reuse_port = false };
    if (auto error = client.with_settings(settings)) {
        std::cerr << error.value() << std::endl;
        return EXIT_FAILURE;
    }
    if (auto error = client.run()) {
        std::cerr << error.value() << std::endl;
        return EXIT_FAILURE;
    }
    uint64_t connectedCount { 0 };
    std::atomic<uint64_t> closedCount { 0 };
    for (std::size_t i = 0; i < (std::size_t)times; i++) {
        auto res = client.tcp_connect(std::get<0>(address));
        if (res.index() == 1) {
            std::cerr << std::get<1>(res) << std::endl;
            client.stop();
            return EXIT_FAILURE;
        }
        connectedCount++;
        auto socket = std::get<0>(res);
        std::shared_ptr<uint8_t[]> buffer { new uint8_t[payload_size] };
        memset(buffer.get(), 'a', payload_size - 1);
        memset(buffer.get() + payload_size - 1, '\0', 1);
        socket->async_write_some(buffer.get(), payload_size,
        [&closedCount, socket, buffer, payload_size](const spinet::Result& res, std::size_t size) {
            socket->async_read_some(
            buffer.get(), payload_size, [&closedCount, socket, buffer](const spinet::Result& res, std::size_t size) {
                socket->close();
                closedCount++;
            });
        });
    }
    std::this_thread::sleep_for(std::chrono::seconds { 30 });
    client.stop();
    std::cerr << "connected: " << connectedCount << ", closed: " << closedCount << std::endl;
    return EXIT_SUCCESS;
}