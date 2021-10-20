#include <cstring>

#include "errno.h"
#include "unistd.h"

#include "fmt/format.h"

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
, settings_ { Settings::default_settings() } {
}

Client::~Client() {
    stop();
    while (running_) {
    }
}

std::optional<std::string> Client::with_settings(Settings settings) {
    if (running_) {
        return "client has been running";
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

std::variant<std::shared_ptr<TcpSocket>, std::string> Client::tcp_connect(const std::string& ip, uint16_t port) {
    if (!running_) {
        return "client is not running";
    }
    ::sockaddr_in address {};
    if (auto err = to_sockaddr_in(address, ip.c_str(), port)) {
        return err.value();
    }
    int fd = ::socket(address.sin_family, SOCK_STREAM, 0);
    if (fd == -1) {
        return fmt::format("socket cannot be created, reason:{}", std::strerror(errno));
    }
    if (settings_.reuse_port) {
        set_reuse_port(fd);
    }
    if (::connect(fd, reinterpret_cast<::sockaddr*>(&address), sizeof(address)) == -1) {
        std::string err = fmt::format("socket cannot be listened, reason:{}", std::strerror(errno));
        ::close(fd);
        return err;
    }
    set_nonblock(fd);
    std::unique_lock<std::mutex> lck { mtx_ };
    std::shared_ptr<TcpSocket> socket { new TcpSocket(fd) };
    select_runtime()->register_handle(socket);
    return socket;
}

void Client::run() {
    std::unique_lock<std::mutex> lck { mtx_ };
    if (running_) {
        return;
    }
    for (std::size_t i = 0; i < settings_.workers; i++) {
        auto& [runtime, thread] = workers_[i];
        thread = std::thread { std::bind(&Runtime::run, runtime) };
    }
    running_ = true;
}

void Client::stop() {
    std::unique_lock<std::mutex> lck { mtx_ };
    if (!running_) {
        return;
    }
    for (std::size_t i = 0; i < settings_.workers; i++) {
        workers_[i].first->stop();
    }
    for (auto& [runtime, thread] : workers_) {
        thread.join();
    }
    workers_.clear();
    running_ = false;
}

bool Client::is_running() {
    return running_;
}

std::shared_ptr<spinet::Runtime>& Client::select_runtime() {
    auto index = (choosen_index_++) % workers_.size();
    return std::get<0>(workers_[index]);
}