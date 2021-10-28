#include <cstdint>
#include <cstring>

#include "sys/epoll.h"
#include "unistd.h"

#include "runtime.h"

using namespace spinet;

constexpr uint32_t EPOLL_WAIT_SIZE = 128;

Runtime::Runtime()
: running_ { false }
, stopped_ { true } {
    epoll_fd_ = ::epoll_create(1); // the arg of epoll_create is deprecated
}

Runtime::~Runtime() {
    stop();
    while (running_) {
    }
    ::close(epoll_fd_);
}

std::optional<std::string> Runtime::run() {
    bool expected = false;
    bool ok = running_.compare_exchange_weak(expected, true);
    if (!ok) {
        return "the server is running";
    }
    stopped_ = false;
    std::optional<std::string> err {};
    ::epoll_event events[EPOLL_WAIT_SIZE];
    while (!stopped_) {
        int event_size = ::epoll_wait(epoll_fd_, events, EPOLL_WAIT_SIZE, 1);
        if (event_size == -1) {
            err.emplace(std::strerror(errno));
            stopped_ = true;
            break;
        }
        for (int i = 0; i < event_size; i++) {
            ::epoll_event& ev = events[i];
            Handle* handle = static_cast<Handle*>(ev.data.ptr);
            if (BaseAcceptor* acceptor = dynamic_cast<BaseAcceptor*>(handle)) {
                if (ev.events & EPOLLIN) {
                    acceptor->do_accept();
                } else {
                    acceptor->close();
                }
                continue;
            }
            if (BaseSocket* socket = dynamic_cast<BaseSocket*>(handle)) {
                if (ev.events & (EPOLLIN | EPOLLPRI)) {
                    socket->do_read();
                } else if (ev.events & EPOLLOUT) {
                    socket->do_write();
                } else {
                    socket->close();
                }
                continue;
            }
        }
        {
            Set<BaseSocket*> all_sockets_copy {};
            {
                std::unique_lock<std::mutex> lck { handles_mtx_ };
                all_sockets_copy = all_sockets_;
            }
            for (auto socket : all_sockets_copy) {
                socket->do_read();
                socket->do_write();
            }
        }
        {
            std::unique_lock<std::mutex> lck { handles_mtx_ };
            for (auto& removable_handle : removable_handles_) {
                removable_handle->close();
            }
            removable_handles_.clear();
        }
    }
    // clear the resources left
    release_all_handles();
    running_ = false;
    return err;
}

void Runtime::stop() {
    if (running_) {
        stopped_ = true;
    }
}

bool Runtime::is_running() {
    return running_;
}

std::size_t Runtime::current_load() {
    std::unique_lock<std::mutex> lck { handles_mtx_ };
    return all_handles_.size();
}

void Runtime::register_handle(std::shared_ptr<Handle> handle) {
    Handle* raw_handle = handle.get();
    raw_handle->runtime_ = weak_from_this();
    int handle_fd = raw_handle->fd_;
    ::epoll_event ev { 0, { 0 } };
    ev.data.ptr = raw_handle;
    std::unique_lock<std::mutex> handles_lck { handles_mtx_ };
    auto prev = all_handles_.find(handle_fd);
    if (prev == all_handles_.end()) {
        // insert
        if (auto socket = dynamic_cast<BaseSocket*>(raw_handle)) {
            ev.events = EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLET | EPOLLRDHUP;
            all_sockets_.insert(socket);
        } else {
            ev.events = EPOLLIN | EPOLLRDHUP;
        }
        ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, handle_fd, &ev);
        all_handles_.insert({ handle_fd, handle });
    } else {
        // pre-remove the old handle
        auto prev_handle = prev->second;
        removable_handles_.push_back(prev_handle);
        Handle* prev_raw_handle = prev_handle.get();
        prev_raw_handle->runtime_.reset(); // prevent the recursive call for deregister_handle
        if (auto socket = dynamic_cast<BaseSocket*>(prev_raw_handle)) {
            all_sockets_.erase(socket);
        }
        // update
        if (auto socket = dynamic_cast<BaseSocket*>(raw_handle)) {
            ev.events = EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLET | EPOLLRDHUP;
            all_sockets_.insert(socket);
        } else {
            ev.events = EPOLLIN | EPOLLRDHUP;
        }
        ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, handle_fd, &ev);
        prev.value() = handle;
    }
}

void Runtime::deregister_handle(Handle* handle) {
    int handle_fd = handle->fd_;
    std::unique_lock<std::mutex> handles_lck { handles_mtx_ };
    auto find = all_handles_.find(handle_fd);
    if (find == all_handles_.end() || find->second.get() != handle) {
        return;
    }
    // pre-remove the old handle and delete the epoll_event
    handle->runtime_.reset(); // prevent the recursive call for deregister_handle
    ::epoll_event ev { 0, { 0 } };
    ev.data.ptr = handle;
    if (auto socket = dynamic_cast<BaseSocket*>(handle)) {
        ev.events = EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLET | EPOLLRDHUP;
        all_sockets_.erase(socket);
    } else {
        ev.events = EPOLLIN | EPOLLRDHUP;
    }
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, handle_fd, &ev);
    removable_handles_.push_back(find->second);
    all_handles_.erase(find);
}

void Runtime::release_all_handles() {
    std::unique_lock<std::mutex> lck { handles_mtx_ };
    for (auto& [fd, handle] : all_handles_) {
        handle->runtime_.reset();
        handle->close();
    }
    for (auto& removable_handle : removable_handles_) {
        removable_handle->close();
    }
    all_handles_.clear();
    all_sockets_.clear();
    removable_handles_.clear();
}