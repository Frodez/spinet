#include <cstring>
#include <iostream>

#include "spinet.h"

long payload_size = 0;

void read_callback(spinet::Error err, std::size_t size, std::shared_ptr<spinet::TcpSocket> socket, std::shared_ptr<uint8_t[]> buffer);

void write_callback(spinet::Error err, std::size_t size, std::shared_ptr<spinet::TcpSocket> socket, std::shared_ptr<uint8_t[]> buffer);

void read_callback(spinet::Error err, std::size_t size, std::shared_ptr<spinet::TcpSocket> socket, std::shared_ptr<uint8_t[]> buffer) {
    if (err) {
        // std::cout << err.to_string() << std::endl;
        socket->close();
        return;
    }
    // std::cout << "read:" << std::string_view { (char*)buffer.get(), size } << std::endl;
    socket->async_write_some(
    buffer.get(), size, std::bind(write_callback, std::placeholders::_1, std::placeholders::_2, socket, buffer));
}

void write_callback(spinet::Error err, std::size_t size, std::shared_ptr<spinet::TcpSocket> socket, std::shared_ptr<uint8_t[]> buffer) {
    if (err) {
        // std::cout << err.to_string() << std::endl;
        socket->close();
        return;
    }
    socket->async_read_some(
    buffer.get(), size, std::bind(read_callback, std::placeholders::_1, std::placeholders::_2, socket, buffer));
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cout << "Usage: PROGRAM_NAME ${address} ${port} ${worker_threads} ${clients} ${payload_size}" << std::endl;
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
    ptr = nullptr;
    long worker_threads = strtol(argv[3], &ptr, 10);
    if (*ptr != 0) {
        std::cout << "worker_threads is an invalid number." << std::endl;
        return EXIT_FAILURE;
    }
    ptr = nullptr;
    long clients = strtol(argv[4], &ptr, 10);
    if (*ptr != 0) {
        std::cout << "clients is an invalid number." << std::endl;
        return EXIT_FAILURE;
    }
    if (clients < 1) {
        std::cout << "clients should be greater than 0." << std::endl;
        return EXIT_FAILURE;
    }
    ptr = nullptr;
    payload_size = strtol(argv[5], &ptr, 10);
    if (*ptr != 0) {
        std::cout << "payload_size is an invalid number." << std::endl;
        return EXIT_FAILURE;
    }
    spinet::Client client {};
    spinet::Client::Settings settings { workers: (uint16_t)worker_threads, reuse_port: false };
    if (auto error = client.with_settings(settings)) {
        std::cout << error.value() << std::endl;
        return EXIT_FAILURE;
    }
    client.run();
    for (std::size_t i = 0; i < (std::size_t)clients; i++) {
        auto res = client.tcp_connect(argv[1], port);
        if (res.index() == 1) {
            std::cout << std::get<1>(res) << std::endl;
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