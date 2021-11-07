#include <cstring>

#include "errno.h"
#include "unistd.h"

#include "core/runtime.h"
#include "util.h"

#include "spinet/client.h"

using namespace spinet;

std::optional<std::string> Client::Settings::validate() {
    if (workers < 1) {
        return "workers must be more than zero";
    }
    return {};
}

Client::Settings Client::Settings::default_settings() {
    return { workers: 1, reuse_port: false };
}

Client::Client()
: running_ { false }
, choosen_index_ { 0 }
, settings_ {} {
}

Client::~Client() {
    stop();
    while (running_) {
    }
}

std::optional<std::string> Client::with_settings(Settings settings) {
    std::unique_lock<std::mutex> lck { mtx_ };
    if (running_) {
        return "client has been running";
    }
    if (settings_) {
        return "settings has been set";
    }
    if (auto err = settings.validate()) {
        return err;
    }
    settings_ = settings;
    for (std::size_t i = 0; i < settings_->workers; i++) {
        workers_.push_back(std::shared_ptr<Runtime> { new Runtime() });
    }
    return {};
}

std::variant<std::shared_ptr<TcpSocket>, std::string> Client::tcp_connect(Address& address) {
    std::unique_lock<std::mutex> lck { mtx_ };
    if (!running_) {
        return "client is not running";
    }
    if (!settings_) {
        return "settings has been not set";
    }
    auto res = to_sockaddr_in(address.address().c_str(), address.port());
    if (res.index() == 1) {
        return std::get<1>(res);
    }
    ::sockaddr_in socket_address = std::get<0>(res);
    int fd = ::socket(socket_address.sin_family, SOCK_STREAM, 0);
    if (fd == -1) {
        return std::string { "socket cannot be created, reason:" } + std::strerror(errno);
    }
    if (settings_->reuse_port) {
        set_reuse_port(fd);
    }
    if (::connect(fd, reinterpret_cast<::sockaddr*>(&socket_address), sizeof(socket_address)) == -1) {
        std::string err = std::string { "socket cannot be listened, reason:" } + std::strerror(errno);
        ::close(fd);
        return err;
    }
    set_nonblock(fd);
    std::shared_ptr<TcpSocket> socket { new TcpSocket(
    fd, std::get<0>(Address::parse(from_sockaddr_in(socket_address), ntohs(socket_address.sin_port)))) };
    select_runtime()->register_handle(socket);
    return socket;
}

std::optional<std::string> Client::run() {
    bool expected = false;
    if (!running_.compare_exchange_weak(expected, true)) {
        return "client has been running";
    }
    std::unique_lock<std::mutex> lck { mtx_ };
    if (!settings_) {
        return "settings has been not set";
    }
    for (auto& worker : workers_) {
        if (auto err = worker->run()) {
            return err;
        }
    }
    return {};
}

void Client::stop() {
    std::unique_lock<std::mutex> lck { mtx_ };
    if (!running_) {
        return;
    }
    for (auto& worker : workers_) {
        worker->stop();
    }
    running_ = false;
}

bool Client::is_running() {
    return running_;
}

std::shared_ptr<spinet::Runtime>& Client::select_runtime() {
    auto index = (choosen_index_++) % workers_.size();
    return workers_[index];
}