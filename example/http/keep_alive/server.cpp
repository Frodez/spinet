#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string_view>

#include "httpparser/httprequestparser.h"
#include "httpparser/request.h"
#include "util.h"

#include "spinet.h"

static const char A[] = "a";

static const char RESPONSE_CONNECTION_CLOSED[] = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/html; "
                                                 "charset=utf-8\r\nContent-Length: 14\r\n\r\nHello, world!";

static const char RESPONSE_KEEP_ALIVE[] = "HTTP/1.1 200 OK\r\nConnection: Keep-Alive\r\nContent-Type: text/html; "
                                          "charset=utf-8\r\nContent-Length: 14\r\n\r\nHello, world!";

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
    public:
    HttpConnection(std::shared_ptr<spinet::TcpSocket> socket, spinet::Timer& timer)
    : socket_ { socket }
    , timer_ { timer }
    , header_ { new uint8_t[256] }
    , header_cap_ { 256 }
    , header_len_ { 0 }
    , parser_ {}
    , request_ {} {};
    ~HttpConnection() {
        socket_->close();
    }
    void start() {
        last_receive_time_point_ = std::chrono::steady_clock::now();
        check_persistent_connection();
        receive_request_header();
    };
    bool is_closed() {
        return socket_->is_closed();
    }
    void close() {
        socket_->close();
    }

    private:
    void check_persistent_connection() {
        if (spinet::Timer::Duration { 2000 } <= std::chrono::steady_clock::now() - last_receive_time_point_) {
            socket_->close();
            return;
        }
        auto self = shared_from_this();
        timer_.async_wait_for(spinet::Timer::Duration { 100 },
        [this, self](spinet::Timer::TimePoint prev, spinet::Timer::TimePoint now) { check_persistent_connection(); });
    }
    void receive_request_header() {
        auto buf = header_.get() + header_len_;
        auto len = header_cap_ - header_len_;
        auto self = shared_from_this();
        socket_->async_read_some(buf, len, [this, self](spinet::Result res, std::size_t len) {
            if (res) {
                socket_->close();
                return;
            }
            last_receive_time_point_ = std::chrono::steady_clock::now();
            set_header_length(len);
            auto header_index = index_of_header_end();
            if (header_index == header_len_) {
                resize_header_buffer();
                receive_request_header();
                return;
            }
            char* begin = (char*)header_.get();
            char* end = (char*)header_.get() + header_len_;
            if (parser_.parse(request_, begin, end) == httpparser::HttpRequestParser::ParsingCompleted) {
                for (auto& item : request_.headers) {
                    if (item.name == "Content-Length") {
                        // receive body
                        char* ptr = nullptr;
                        body_len_ = strtol(item.value.c_str(), &ptr, 10);
                        if (*ptr != 0) {
                            // invalid number
                            break;
                        }
                        body_ = std::shared_ptr<uint8_t[]> { new uint8_t[body_len_] };
                        socket_->async_read(body_.get(), body_len_, [this, self](spinet::Result res, std::size_t len) {
                            if (res) {
                                socket_->close();
                                return;
                            }
                            last_receive_time_point_ = std::chrono::steady_clock::now();
                            send_response();
                        });
                        return;
                    }
                }
            }
            send_response();
        });
    };
    void send_response() {
        auto self = shared_from_this();
        socket_->async_write(request_.keepAlive ? (uint8_t*)RESPONSE_KEEP_ALIVE : (uint8_t*)RESPONSE_CONNECTION_CLOSED,
        request_.keepAlive ? sizeof(RESPONSE_KEEP_ALIVE) : sizeof(RESPONSE_CONNECTION_CLOSED),
        [this, self](spinet::Result res, std::size_t len) {
            if (res) {
                socket_->close();
                return;
            }
            if (!request_.keepAlive) {
                socket_->close();
            } else {
                header_ = std::shared_ptr<uint8_t[]> { new uint8_t[256] };
                header_cap_ = 256;
                header_len_ = 0;
                receive_request_header();
            }
        });
    };
    void set_header_length(std::size_t len) {
        header_len_ = header_len_ + len;
    }
    std::size_t index_of_header_end() {
        uint8_t header_end[] = { '\r', '\n', '\r', '\n' };
        uint8_t* begin = header_.get();
        uint8_t* index_ptr = std::search(header_end, header_end + sizeof(header_end), begin, begin + header_len_);
        return index_ptr - begin;
    };
    void resize_header_buffer() {
        if (header_len_ != header_cap_) {
            return;
        }
        std::shared_ptr<uint8_t[]> header { new uint8_t[header_cap_ * 2] };
        memcpy(header.get(), header_.get(), header_len_);
        header_ = header;
        header_cap_ = header_cap_ * 2;
    }
    std::shared_ptr<spinet::TcpSocket> socket_;
    spinet::Timer& timer_;
    std::shared_ptr<uint8_t[]> header_;
    std::size_t header_cap_;
    std::size_t header_len_;
    std::shared_ptr<uint8_t[]> body_;
    std::size_t body_len_;
    httpparser::HttpRequestParser parser_;
    httpparser::Request request_;
    std::chrono::steady_clock::time_point last_receive_time_point_;
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: PROGRAM_NAME ${address} ${port} ${worker_threads}" << std::endl;
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
    spinet::Server server {};
    spinet::Server::Settings settings { workers: (uint16_t)worker_threads, reuse_port: false };
    auto error = server.with_settings(settings);
    if (error) {
        std::cerr << error.value() << std::endl;
        return EXIT_FAILURE;
    }
    spinet::Timer timer {};
    timer.run();
    error = server.listen_tcp_endpoint(std::get<0>(address), [&timer](std::shared_ptr<spinet::TcpSocket> socket) {
        auto connection = std::shared_ptr<HttpConnection>(new HttpConnection(socket, timer));
        connection->start();
    });
    if (error) {
        std::cerr << error.value() << std::endl;
        return EXIT_FAILURE;
    }
    std::thread t { [&server]() { server.run(); } };
    std::this_thread::sleep_for(std::chrono::seconds { 60 });
    timer.stop();
    server.stop();
    t.join();
    return EXIT_SUCCESS;
}