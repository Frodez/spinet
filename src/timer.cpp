#include "spinet/timer.h"

using namespace spinet;

constexpr Timer::Duration MINIMUM_PRECISION = std::chrono::milliseconds { 1 };

Timer::Timer(Duration precision)
: running_ { false }
, stopped_ { true } {
    if (precision < MINIMUM_PRECISION) {
        precision_ = MINIMUM_PRECISION;
    } else {
        precision_ = std::chrono::duration_cast<Duration>(precision);
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
    timer_thread_ = std::thread { &Timer::exec, this };
}

void Timer::stop() {
    if (running_) {
        stopped_ = true;
        while (running_) {
            waiter_cv_.notify_all();
        }
        timer_thread_.join();
    }
}

bool Timer::is_running() {
    return running_;
}

void Timer::exec() {
    stopped_ = false;
    while (!stopped_) {
        TimePoint before_time_point {};
        {
            std::unique_lock<std::mutex> lck { waiter_mtx_ };
            before_time_point = std::chrono::steady_clock::now();
            auto lower = waiters_.begin();
            auto upper = waiters_.upper_bound(before_time_point);
            std::vector<std::pair<TimePoint, Callback>> callbacks { lower, upper };
            waiters_.erase(lower, upper);
            lck.unlock();
            for (auto& [time_point, callback] : callbacks) {
                callback();
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
        Duration duration = std::chrono::duration_cast<Duration>(std::chrono::steady_clock::now() - before_time_point);
        if (precision_ > duration) {
            std::this_thread::sleep_for(precision_ - duration);
        }
    }
    std::unique_lock<std::mutex> lck { waiter_mtx_ };
    waiters_.clear();
    running_ = false;
}

void Timer::async_wait_for(Duration duration, Callback callback) {
    async_wait_until(std::chrono::steady_clock::now() + duration, callback);
}

void Timer::async_wait_until(TimePoint time_point, Callback callback) {
    std::unique_lock<std::mutex> lck { waiter_mtx_ };
    if (waiters_.empty()) {
        waiters_.insert({ time_point, callback });
        waiter_cv_.notify_one();
    } else {
        waiters_.insert({ time_point, callback });
    }
}