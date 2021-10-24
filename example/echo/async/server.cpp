#include <iostream>

#include "spinet.h"

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
    if (argc != 5) {
        std::cout << "Usage: PROGRAM_NAME ${address} ${port} ${worker_threads} ${payload_size}" << std::endl;
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
    payload_size = strtol(argv[4], &ptr, 10);
    if (*ptr != 0) {
        std::cout << "payload_size is an invalid number." << std::endl;
        return EXIT_FAILURE;
    }
    spinet::Server server {};
    spinet::Server::Settings settings { workers: (uint16_t)worker_threads, reuse_port: false };
    auto error = server.with_settings(settings);
    if (error) {
        std::cout << error.value() << std::endl;
        return EXIT_FAILURE;
    }
    error = server.listen_tcp_endpoint(argv[1], port, [](std::shared_ptr<spinet::TcpSocket> socket) {
        std::shared_ptr<uint8_t[]> buffer { new uint8_t[payload_size] };
        socket->async_read_some(buffer.get(), payload_size,
        std::bind(read_callback, std::placeholders::_1, std::placeholders::_2, socket, buffer));
    });
    if (error) {
        std::cout << error.value() << std::endl;
        return EXIT_FAILURE;
    }
    std::thread t { [&server]() { server.run(); } };
    std::this_thread::sleep_for(std::chrono::seconds { 60 });
    server.stop();
    t.join();
    return EXIT_SUCCESS;
}