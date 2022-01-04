#include <utility>

#include "spinet/timer.h"

using namespace spinet;

constexpr Timer::Duration MINIMUM_PRECISION = Timer::Duration { 1 };

Timer::Timer(Duration precision)
: running_ { false }
, stopped_ { true } {
    if (precision < MINIMUM_PRECISION) {
        precision_ = MINIMUM_PRECISION;
    } else {
        precision_ = std::chrono::duration_cast<Duration>(precision - (precision_ % MINIMUM_PRECISION));
    }
}

Timer::~Timer() {
    stop();
}

void Timer::run() {
    bool expected = false;
    if (!running_.compare_exchange_weak(expected, true)) {
        return;
    }
    std::unique_lock<std::mutex> lck { thread_mtx_ };
    timer_thread_ = std::thread { &Timer::exec, this };
}

void Timer::stop() {
    if (running_) {
        stopped_ = true;
        while (running_) {
            waiter_cv_.notify_all();
        }
        std::unique_lock<std::mutex> lck { thread_mtx_ };
        timer_thread_.join();
    }
}

bool Timer::is_running() {
    return running_;
}

void Timer::exec() {
    stopped_ = false;
    while (!stopped_) {
        TimePoint time_point {};
        {
            std::unique_lock<std::mutex> lck { waiter_mtx_ };
            time_point = std::chrono::steady_clock::now();
            std::vector<WaitOperation> operations {};
            while (!waiters_.empty()) {
                auto& operation = waiters_.top();
                if (operation.time_point > time_point) {
                    break;
                }
                operations.push_back(operation);
                waiters_.pop();
            }
            lck.unlock();
            for (auto& [prev_time_point, callback] : operations) {
                callback(prev_time_point, time_point);
            }
        }
        {
            std::unique_lock<std::mutex> lck { waiter_mtx_ };
            if (waiters_.empty()) {
                waiter_cv_.wait(lck);
                if (stopped_) {
                    break;
                }
            }
        }
        auto duration = std::chrono::steady_clock::now() - time_point;
        if (precision_ > duration) {
            std::this_thread::sleep_for(precision_ - duration);
        }
    }
    std::unique_lock<std::mutex> lck { waiter_mtx_ };
    {
        std::priority_queue<WaitOperation, std::vector<WaitOperation>> empty_queue {};
        waiters_.swap(empty_queue);
    }
    running_ = false;
}

void Timer::async_wait_for(Duration duration, const Callback& callback) {
    async_wait_until(std::chrono::steady_clock::now() + duration, callback);
}

void Timer::async_wait_until(TimePoint time_point, const Callback& callback) {
    std::unique_lock<std::mutex> lck { waiter_mtx_ };
    if (waiters_.empty()) {
        waiters_.push({ time_point, callback });
        waiter_cv_.notify_one();
    } else {
        waiters_.push({ time_point, callback });
    }
}