#include <cstring>
#include <iostream>

#include "spinet.h"
#include "util.h"

long payload_size = 0;

void read_callback(spinet::Result res, std::size_t size, std::shared_ptr<spinet::TcpSocket> socket, std::shared_ptr<uint8_t[]> buffer);

void write_callback(spinet::Result res, std::size_t size, std::shared_ptr<spinet::TcpSocket> socket, std::shared_ptr<uint8_t[]> buffer);

void read_callback(spinet::Result res, std::size_t size, std::shared_ptr<spinet::TcpSocket> socket, std::shared_ptr<uint8_t[]> buffer) {
    if (res) {
        socket->close();
        return;
    }
    socket->async_write_some(
    buffer.get(), size, std::bind(write_callback, std::placeholders::_1, std::placeholders::_2, socket, buffer));
}

void write_callback(spinet::Result res, std::size_t size, std::shared_ptr<spinet::TcpSocket> socket, std::shared_ptr<uint8_t[]> buffer) {
    if (res) {
        socket->close();
        return;
    }
    socket->async_read_some(
    buffer.get(), size, std::bind(read_callback, std::placeholders::_1, std::placeholders::_2, socket, buffer));
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: PROGRAM_NAME ${address} ${port} ${worker_threads} ${clients} ${payload_size}" << std::endl;
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
    auto clients = expectInt(argv[4], "clients is an invalid number.");
    if (clients <= 0) {
        std::cerr << "clients should be greater than 0." << std::endl;
        return EXIT_FAILURE;
    }
    auto payload_size = expectInt(argv[5], "payload_size is an invalid number.");
    if (payload_size <= 0) {
        std::cerr << "payload_size should be greater than 0." << std::endl;
        return EXIT_FAILURE;
    }
    spinet::Client client {};
    spinet::Client::Settings settings { workers: (uint16_t)worker_threads, reuse_port: false };
    if (auto error = client.with_settings(settings)) {
        std::cerr << error.value() << std::endl;
        return EXIT_FAILURE;
    }
    if (auto error = client.run()) {
        std::cerr << error.value() << std::endl;
        return EXIT_FAILURE;
    }
    for (std::size_t i = 0; i < (std::size_t)clients; i++) {
        auto res = client.tcp_connect(std::get<0>(address));
        if (res.index() == 1) {
            std::cerr << std::get<1>(res) << std::endl;
            client.stop();
            return EXIT_FAILURE;
        }
        auto socket = std::get<0>(res);
        std::shared_ptr<uint8_t[]> buffer { new uint8_t[payload_size] };
        memset(buffer.get(), 'a', payload_size - 1);
        memset(buffer.get() + payload_size - 1, '\0', 1);
        socket->async_write_some(buffer.get(), payload_size,
        std::bind(write_callback, std::placeholders::_1, std::placeholders::_2, socket, buffer));
    }
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds { 1 });
    };
    client.stop();
    return EXIT_SUCCESS;
}