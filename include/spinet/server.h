#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
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
    bool has_settings();

    std::optional<std::string>
    listen_tcp_endpoint(Address& address, const std::function<void(std::shared_ptr<TcpSocket>)>& accept_callback);

    std::optional<std::string> run();
    void stop();
    bool is_running();

    private:
    using Worker = std::shared_ptr<Runtime>;

    Server(Server&& other) = delete;
    Server& operator=(Server&& other) = delete;
    Server(const Server& other) = delete;
    Server& operator=(const Server& other) = delete;

    std::mutex mtx_;
    std::atomic<bool> running_;
    std::vector<Worker> workers_;

    std::optional<Settings> settings_;
};

}