#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>

#include "core/tcp_socket.h"

namespace spinet {

class Client {
    public:
    struct Settings {
        uint16_t workers;
        bool reuse_port;
        static Settings default_settings();
        std::optional<std::string> validate();
    };

    Client();
    ~Client();

    std::optional<std::string> with_settings(Settings settings);
    std::variant<std::shared_ptr<TcpSocket>, std::string> tcp_connect(const std::string& ip, uint16_t port);

    void run();
    void stop();
    bool is_running();

    private:
    using Worker = std::pair<std::shared_ptr<Runtime>, std::thread>;

    Client(Client&& other) = delete;
    Client& operator=(Client&& other) = delete;
    Client(const Client& other) = delete;
    Client& operator=(const Client& other) = delete;

    std::shared_ptr<spinet::Runtime>& select_runtime();

    std::mutex mtx_;
    std::atomic<bool> running_;
    std::vector<Worker> workers_;

    std::atomic<uint64_t> choosen_index_;

    Settings settings_;
};

}