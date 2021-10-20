#include <cstring>

#include "errno.h"
#include "unistd.h"

#include "fmt/format.h"

#include "core/runtime.h"
#include "util.h"

#include "spinet/server.h"

namespace spinet {

class TcpAcceptor : public BaseAcceptor {
    public:
    TcpAcceptor(int fd, std::weak_ptr<Runtime> runtime, Server::Settings* settings, std::function<void(std::shared_ptr<TcpSocket>)> accept_callback) {
        fd_ = fd;
        runtime_ = runtime;
        settings_ = settings;
        accept_callback_ = accept_callback;
    }
    ~TcpAcceptor() {
    }

    protected:
    void do_accept() override {
        ::sockaddr_in socket_address {};
        ::socklen_t address_size = sizeof(socket_address);
        while (true) {
            int socket_fd = ::accept(fd_, (::sockaddr*)&socket_address, &address_size);
            if (socket_fd == -1) {
                return;
            }
            set_nonblock(socket_fd);
            if (settings_->reuse_port) {
                set_reuse_port(socket_fd);
            }
            std::shared_ptr<TcpSocket> socket { new TcpSocket(socket_fd) };
            if (auto runtime = runtime_.lock()) {
                runtime->register_handle(socket);
            }
            accept_callback_(socket);
        }
    }

    Server::Settings* settings_;
    std::function<void(std::shared_ptr<TcpSocket>)> accept_callback_;
};

}

using namespace spinet;

std::optional<std::string> Server::Settings::validate() {
    if (workers < 1) {
        return "workers must be more than zero";
    }
    return {};
}

Server::Settings Server::Settings::default_settings() {
    return { workers: 1, reuse_port: false };
}

Server::Server()
: running_ { false }
, settings_ { Settings::default_settings() } {
}

Server::~Server() {
    stop();
    while (running_) {
    }
}

std::optional<std::string> Server::with_settings(Settings settings) {
    if (running_) {
        return "server has been running";
    }
    if (auto err = settings.validate()) {
        return err;
    }
    settings_ = settings;
    std::unique_lock<std::mutex> lck { mtx_ };
    for (std::size_t i = 0; i < settings_.workers; i++) {
        std::shared_ptr<Runtime> runtime { new Runtime() };
        workers_.push_back({ runtime, std::thread {} });
    }
    return {};
}

std::optional<std::string>
Server::listen_tcp_endpoint(const std::string& ip, uint16_t port, std::function<void(std::shared_ptr<TcpSocket>)> accept_callback) {
    {
        std::unique_lock<std::mutex> lck { mtx_ };
        if (running_) {
            return "server has been running";
        }
    }
    ::sockaddr_in address {};
    if (auto err = to_sockaddr_in(address, ip.c_str(), port)) {
        return err;
    }
    std::vector<int> listen_fds {};
    std::unique_lock<std::mutex> lck { mtx_ };
    for (std::size_t i = 0; i < workers_.size(); i++) {
        int listen_fd = ::socket(address.sin_family, SOCK_STREAM, 0);
        if (listen_fd == -1) {
            std::string err = fmt::format("socket cannot open, reason:{}", std::strerror(errno));
            for (std::size_t i = 0; i < listen_fds.size(); i++) {
                ::close(listen_fds[i]);
            }
            return err;
        }
        set_reuse_port(listen_fd);
        listen_fds.push_back(listen_fd);
        if (::bind(listen_fd, reinterpret_cast<::sockaddr*>(&address), sizeof(address)) != 0) {
            std::string err =
            fmt::format("socket cannot bind with address{}, reason:{}", from_sockaddr_in(address), std::strerror(errno));
            for (std::size_t i = 0; i < listen_fds.size(); i++) {
                ::close(listen_fds[i]);
            }
            return err;
        }
        if (::listen(listen_fd, SOMAXCONN) == -1) {
            std::string err = fmt::format("socket cannot be listened, reason:{}", std::strerror(errno));
            for (std::size_t i = 0; i < listen_fds.size(); i++) {
                ::close(listen_fds[i]);
            }
            return err;
        }
        set_nonblock(listen_fd);
    }
    for (std::size_t i = 0; i < listen_fds.size(); i++) {
        auto& [runtime, thread] = workers_[i];
        std::shared_ptr<TcpAcceptor> acceptor { new TcpAcceptor(listen_fds[i], runtime, &settings_, accept_callback) };
        runtime->register_handle(acceptor);
    }
    return {};
}

void Server::run() {
    bool expected = false;
    if (!running_.compare_exchange_weak(expected, true)) {
        return;
    }
    {
        std::unique_lock<std::mutex> lck { mtx_ };
        for (std::size_t i = 0; i < settings_.workers; i++) {
            auto& [runtime, thread] = workers_[i];
            thread = std::thread { std::bind(&Runtime::run, runtime) };
        }
    }
    for (auto& [runtime, thread] : workers_) {
        thread.join();
    }
    {
        std::unique_lock<std::mutex> lck { mtx_ };
        workers_.clear();
    }
    running_ = false;
}

void Server::stop() {
    if (!running_) {
        return;
    }
    std::unique_lock<std::mutex> lck { mtx_ };
    for (std::size_t i = 0; i < settings_.workers; i++) {
        workers_[i].first->stop();
    }
}

bool Server::is_running() {
    return running_;
}