#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "tsl/robin_map.h"
#include "tsl/robin_set.h"

#include "spinet/core/handle.h"

namespace spinet {

class Runtime : public std::enable_shared_from_this<Runtime> {
    public:
    Runtime();
    ~Runtime();
    std::optional<std::string> run();
    void stop();
    bool is_running();
    std::size_t current_load();
    void register_handle(const std::shared_ptr<Handle> &handle);
    void deregister_handle(Handle* handle);

    private:
    template <typename K, typename V> using Map = tsl::robin_map<K, V>;
    template <typename E> using Set = tsl::robin_set<E>;

    void exec();

    void release_all_handles();

    std::atomic<bool> running_;
    std::atomic<bool> stopped_;
    std::mutex thread_mtx_;
    std::thread runtime_thread_;

    int epoll_fd_;

    std::mutex handles_mtx_;
    std::vector<std::shared_ptr<Handle>> removable_handles_;
    Set<BaseSocket*> all_sockets_;
    Map<int, std::shared_ptr<Handle>> all_handles_;
};

}