#include <iostream>

#include "spinet.h"
#include "util.h"

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
        std::cerr << "Usage: PROGRAM_NAME ${address} ${port} ${worker_threads} ${payload_size}" << std::endl;
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
    auto payload_size = expectInt(argv[4], "payload_size is an invalid number.");
    if (payload_size <= 0) {
        std::cerr << "payload_size should be greater than 0." << std::endl;
        return EXIT_FAILURE;
    }
    spinet::Server server {};
    spinet::Server::Settings settings { workers: (uint16_t)worker_threads, reuse_port: false };
    auto error = server.with_settings(settings);
    if (error) {
        std::cerr << error.value() << std::endl;
        return EXIT_FAILURE;
    }
    error = server.listen_tcp_endpoint(std::get<0>(address), [payload_size](std::shared_ptr<spinet::TcpSocket> socket) {
        std::shared_ptr<uint8_t[]> buffer { new uint8_t[payload_size] };
        socket->async_read_some(buffer.get(), payload_size,
        std::bind(read_callback, std::placeholders::_1, std::placeholders::_2, socket, buffer));
    });
    if (error) {
        std::cerr << error.value() << std::endl;
        return EXIT_FAILURE;
    }
    std::thread t { [&server]() { server.run(); } };
    std::this_thread::sleep_for(std::chrono::seconds { 60 });
    server.stop();
    t.join();
    return EXIT_SUCCESS;
}