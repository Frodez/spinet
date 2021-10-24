#include <cstring>
#include <iostream>
#include <memory>

#include "spinet.h"

static const char RESPONSE[] = "HTTP/1.0 200 OK\r\nContent-Length: 13\r\nConnection: close\r\n\r\nHello, world!";

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
    public:
    HttpConnection(std::shared_ptr<spinet::TcpSocket> socket)
    : socket_ { socket }
    , header_ { new uint8_t[1024] }
    , header_cap_ { 1024 }
    , header_len_ { 0 } {};
    ~HttpConnection() {
        socket_->close();
    }
    void start() {
        recv_request_header();
    };
    bool is_closed() {
        return socket_->is_closed();
    }
    void close() {
        socket_->close();
    }

    private:
    void recv_request_header() {
        auto self = shared_from_this();
        socket_->async_read_some(
        header_.get() + header_len_, header_cap_ - header_len_, [this, self](spinet::Result res, std::size_t len) {
            if (res) {
                socket_->close();
                return;
            }
            header_len_ = header_len_ + len;
            auto header_index = index_of_header_end();
            if (header_index == header_len_) {
                if (header_len_ == header_cap_) {
                    std::shared_ptr<uint8_t[]> header { new uint8_t[header_cap_ * 2] };
                    memcpy(header.get(), header_.get(), header_len_);
                    header_ = std::move(header);
                    header_cap_ = header_cap_ * 2;
                }
                recv_request_header();
                return;
            }
            send_response();
        });
    };
    void send_response() {
        auto self = shared_from_this();
        socket_->async_write((uint8_t*)RESPONSE, sizeof(RESPONSE), [this, self](spinet::Result res, std::size_t len) {
            if (res) {
            }
            socket_->close();
        });
    };
    std::size_t index_of_header_end() {
        uint8_t* header = header_.get();
        for (std::size_t index = 0; index < header_len_ - 3; index++) {
            if (header[index] != '\r') {
                continue;
            }
            std::size_t pos = index + 1;
            if (header[pos] != '\n') {
                continue;
            }
            pos = pos + 1;
            if (header[pos] != '\r') {
                continue;
            }
            pos = pos + 1;
            if (header[pos] == '\n') {
                return pos;
            }
        }
        return header_len_;
    };
    std::shared_ptr<spinet::TcpSocket> socket_;
    std::shared_ptr<uint8_t[]> header_;
    std::size_t header_cap_;
    std::size_t header_len_;
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Usage: PROGRAM_NAME ${address} ${port} ${worker_threads}" << std::endl;
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
    auto address = spinet::Address::parse(argv[1], port);
    if (address.index() == 1) {
        std::cout << std::get<1>(address) << std::endl;
        return EXIT_FAILURE;
    }
    ptr = nullptr;
    long worker_threads = strtol(argv[3], &ptr, 10);
    if (*ptr != 0) {
        std::cout << "worker_threads is an invalid number." << std::endl;
        return EXIT_FAILURE;
    }
    spinet::Server server {};
    spinet::Server::Settings settings { workers: (uint16_t)worker_threads, reuse_port: false };
    auto error = server.with_settings(settings);
    if (error) {
        std::cout << error.value() << std::endl;
        return EXIT_FAILURE;
    }
    error = server.listen_tcp_endpoint(std::get<0>(address), [](std::shared_ptr<spinet::TcpSocket> socket) {
        auto connection = std::shared_ptr<HttpConnection>(new HttpConnection(socket));
        connection->start();
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