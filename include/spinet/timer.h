#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

namespace spinet {

class Timer {
    public:
    using Duration = std::chrono::milliseconds;
    using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
    using Callback = std::function<void(TimePoint, TimePoint)>;

    Timer(Duration precision = Duration { 1 });
    ~Timer();

    void run();
    void stop();
    bool is_running();

    void async_wait_for(Duration duration, const Callback& callback);
    void async_wait_until(TimePoint time_point, const Callback& callback);

    private:
    Timer(Timer&& other) = delete;
    Timer& operator=(Timer&& other) = delete;
    Timer(const Timer& other) = delete;
    Timer& operator=(const Timer& other) = delete;

    void exec();

    Timer::Duration precision_;

    std::atomic<bool> running_;
    std::atomic<bool> stopped_;
    std::mutex thread_mtx_;
    std::thread timer_thread_;

    std::mutex waiter_mtx_;
    std::condition_variable waiter_cv_;
    std::multimap<TimePoint, Callback> waiters_;
};

}