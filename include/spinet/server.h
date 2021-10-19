#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "core/tcp_socket.h"

namespace spinet {

class Server {
    public:
    struct Settings {
        uint16_t workers;
        bool reuse_port;
        static Settings default_settings();
        std::optional<std::string> validate();
    };

    Server();
    ~Server();

    std::optional<std::string> with_settings(Settings settings);

    std::optional<std::string>
    listen_tcp_endpoint(const std::string& ip, uint16_t port, std::function<void(std::shared_ptr<TcpSocket>)> accept_callback);

    void run();
    void stop();
    bool is_running();

    private:
    using Worker = std::pair<std::shared_ptr<Runtime>, std::thread>;

    Server(Server&& other) = delete;
    Server& operator=(Server&& other) = delete;
    Server(const Server& other) = delete;
    Server& operator=(const Server& other) = delete;

    std::mutex mtx_;
    std::atomic<bool> running_;
    std::vector<Worker> workers_;

    Settings settings_;
};

}