#include "errno.h"
#include "sys/socket.h"
#include "unistd.h"

#include "core/runtime.h"

#include "spinet/core/tcp_socket.h"

namespace spinet {

enum TaskStrategy { TRY, UNTIL_FINISHED };

class TcpReadTask {
    public:
    TcpReadTask(uint8_t* buf, std::size_t size, spinet::TaskStrategy strategy)
    : finished_ { false }
    , buf_ { buf }
    , size_ { size }
    , pos_ { 0 }
    , strategy_ { strategy } {
    }

    std::optional<int> exec(int fd) {
        if (finished_) {
            return {};
        }
        ssize_t bytes = ::recv(fd, buf_ + pos_, size_ - pos_, MSG_NOSIGNAL);
        if (bytes <= 0) {
            if (bytes == 0) {
                return EBADF;
            } else {
                return errno;
            }
        }
        pos_ = pos_ + bytes;
        if (strategy_ == TaskStrategy::TRY) {
            finished_ = true;
        } else { // the alternative is UNTIL_FINISHED
            if (pos_ == size_) {
                finished_ = true;
            }
        }
        return {};
    }

    bool finished() {
        return finished_;
    }

    std::size_t finished_size() {
        return pos_;
    }

    private:
    bool finished_;
    uint8_t* buf_;
    std::size_t size_;
    std::size_t pos_;
    spinet::TaskStrategy strategy_;
};

class TcpWriteTask {
    public:
    TcpWriteTask(uint8_t* buf, std::size_t size, spinet::TaskStrategy strategy)
    : finished_ { false }
    , buf_ { buf }
    , size_ { size }
    , pos_ { 0 }
    , strategy_ { strategy } {
    }

    std::optional<int> exec(int fd) {
        if (finished_) {
            return {};
        }
        ssize_t bytes = ::send(fd, buf_ + pos_, size_ - pos_, MSG_NOSIGNAL);
        if (bytes <= 0) {
            if (bytes == 0) {
                return EBADF;
            } else {
                return errno;
            }
        }
        pos_ = pos_ + bytes;
        if (strategy_ == TaskStrategy::TRY) {
            finished_ = true;
        } else { // the alternative is UNTIL_FINISHED
            if (pos_ == size_) {
                finished_ = true;
            }
        }
        return {};
    }

    bool finished() {
        return finished_;
    }

    std::size_t finished_size() {
        return pos_;
    }

    private:
    bool finished_;
    uint8_t* buf_;
    std::size_t size_;
    std::size_t pos_;
    spinet::TaskStrategy strategy_;
};

}

using namespace spinet;

TcpSocket::TcpSocket(int fd, Address peer)
: closed_ { false }
, peer_ { peer } {
    fd_ = fd;
}

TcpSocket::~TcpSocket() {
    close();
}

bool TcpSocket::async_read(uint8_t* buf, std::size_t size, ReadCallback callback) {
    std::unique_lock<std::mutex> lck { read_mtx_ };
    if (closed_) {
        return false;
    }
    TcpReadTask task { buf, size, TaskStrategy::UNTIL_FINISHED };
    if (read_task_queue_.empty()) {
        task.exec(fd_);
    }
    read_task_queue_.push_back({ task, callback });
    return true;
}

bool TcpSocket::async_read_some(uint8_t* buf, std::size_t size, ReadCallback callback) {
    std::unique_lock<std::mutex> lck { read_mtx_ };
    if (closed_) {
        return false;
    }
    TcpReadTask task { buf, size, TaskStrategy::TRY };
    if (read_task_queue_.empty()) {
        task.exec(fd_);
    }
    read_task_queue_.push_back({ task, callback });
    return true;
}

bool TcpSocket::async_write(uint8_t* buf, std::size_t size, WriteCallback callback) {
    std::unique_lock<std::mutex> lck { write_mtx_ };
    if (closed_) {
        return false;
    }
    TcpWriteTask task { buf, size, TaskStrategy::UNTIL_FINISHED };
    if (write_task_queue_.empty()) {
        task.exec(fd_);
    }
    write_task_queue_.push_back({ task, callback });
    return true;
}

bool TcpSocket::async_write_some(uint8_t* buf, std::size_t size, WriteCallback callback) {
    std::unique_lock<std::mutex> lck { write_mtx_ };
    if (closed_) {
        return false;
    }
    TcpWriteTask task { buf, size, TaskStrategy::TRY };
    if (write_task_queue_.empty()) {
        task.exec(fd_);
    }
    write_task_queue_.push_back({ task, callback });
    return true;
}

void TcpSocket::cancel() {
    std::scoped_lock lck { read_mtx_, write_mtx_ };
    read_task_queue_.clear();
    write_task_queue_.clear();
}

bool TcpSocket::is_closed() {
    return closed_;
}

void TcpSocket::close() {
    bool expected = false;
    if (!closed_.compare_exchange_weak(expected, true)) {
        return;
    }
    Handle::close();
    {
        std::unique_lock<std::mutex> lck { read_mtx_ };
        while (!read_task_queue_.empty()) {
            auto [task, callback] = read_task_queue_.front();
            read_task_queue_.pop_front();
            lck.unlock();
            callback(Result::system_error(EBADF), task.finished_size());
            lck.lock();
        }
    }
    {
        std::unique_lock<std::mutex> lck { write_mtx_ };
        while (!write_task_queue_.empty()) {
            auto [task, callback] = write_task_queue_.front();
            write_task_queue_.pop_front();
            lck.unlock();
            callback(Result::system_error(EBADF), task.finished_size());
            lck.lock();
        }
    }
}

Address TcpSocket::peer() {
    return peer_;
}

void TcpSocket::do_read() {
    std::unique_lock<std::mutex> lck { read_mtx_ };
    if (closed_) {
        return;
    }
    if (read_task_queue_.empty()) {
        return;
    }
    auto [task, callback] = read_task_queue_.front();
    auto ec = task.exec(fd_);
    if (task.finished()) {
        read_task_queue_.pop_front();
        lck.unlock();
        callback(Result::ok(), task.finished_size());
    } else if (ec && ec.value() != EAGAIN) {
        read_task_queue_.pop_front();
        lck.unlock();
        callback(Result::system_error(ec.value()), task.finished_size());
    }
}

void TcpSocket::do_write() {
    std::unique_lock<std::mutex> lck { write_mtx_ };
    if (closed_) {
        return;
    }
    if (write_task_queue_.empty()) {
        return;
    }
    auto [task, callback] = write_task_queue_.front();
    auto ec = task.exec(fd_);
    if (task.finished()) {
        write_task_queue_.pop_front();
        lck.unlock();
        callback(Result::ok(), task.finished_size());
    } else if (ec && ec.value() != EAGAIN) {
        write_task_queue_.pop_front();
        lck.unlock();
        callback(Result::system_error(ec.value()), task.finished_size());
    }
}