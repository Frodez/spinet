#pragma once

#include <atomic>
#include <functional>
#include <list>
#include <mutex>
#include <utility>

#include "address.h"
#include "handle.h"
#include "result.h"

namespace spinet {

class TcpReadTask;
class TcpWriteTask;

class TcpSocket : public BaseSocket {
    public:
    using ReadCallback = std::function<void(Result, std::size_t)>;
    using WriteCallback = std::function<void(Result, std::size_t)>;

    TcpSocket(int fd, const Address& peer);
    ~TcpSocket();

    bool async_read(uint8_t* buf, std::size_t size, const ReadCallback& callback);
    bool async_read_some(uint8_t* buf, std::size_t size, const ReadCallback& callback);
    bool async_write(uint8_t* buf, std::size_t size, const WriteCallback& callback);
    bool async_write_some(uint8_t* buf, std::size_t size, const WriteCallback& callback);

    void cancel();
    bool is_closed();
    void close() override;
    Address peer();

    private:
    TcpSocket(TcpSocket&& other) = delete;
    TcpSocket& operator=(TcpSocket&& other) = delete;
    TcpSocket(const TcpSocket& other) = delete;
    TcpSocket& operator=(const TcpSocket& other) = delete;

    void do_read() override;
    void do_write() override;

    std::atomic<bool> closed_;

    std::mutex read_mtx_;
    std::list<std::pair<TcpReadTask, ReadCallback>> read_task_queue_;

    std::mutex write_mtx_;
    std::list<std::pair<TcpWriteTask, WriteCallback>> write_task_queue_;

    Address peer_;
};

}