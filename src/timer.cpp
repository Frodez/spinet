#include <list>

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
    timer_thread_.join();
}

void Timer::run() {
    bool expected = false;
    if (!running_.compare_exchange_weak(expected, true)) {
        return;
    }
    timer_thread_.join();
    timer_thread_ = std::thread { &Timer::exec, this };
}

void Timer::stop() {
    if (running_) {
        stopped_ = true;
        while (running_) {
            waiter_cv_.notify_all();
        }
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
            auto lower = waiters_.begin();
            auto upper = waiters_.upper_bound(time_point);
            std::vector<std::pair<TimePoint, Callback>> callbacks { lower, upper };
            waiters_.erase(lower, upper);
            lck.unlock();
            for (auto& [prev_time_point, callback] : callbacks) {
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
    waiters_.clear();
    running_ = false;
}

void Timer::async_wait_for(Duration duration, Callback callback) {
    async_wait_until(std::chrono::steady_clock::now() + duration, callback);
}

void Timer::async_wait_until(TimePoint time_point, Callback callback) {
    auto time_since_epoch = time_point.time_since_epoch();
    TimePoint floored_time_point { time_since_epoch - (time_since_epoch % precision_) };
    std::unique_lock<std::mutex> lck { waiter_mtx_ };
    if (waiters_.empty()) {
        waiters_.insert({ floored_time_point, callback });
        waiter_cv_.notify_one();
    } else {
        waiters_.insert({ floored_time_point, callback });
    }
}