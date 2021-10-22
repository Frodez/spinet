#pragma once

#include <memory>
#include <optional>
#include <string>

namespace spinet {

class Runtime;

class Handle {
    public:
    virtual ~Handle();
    virtual void close();

    protected:
    friend class Runtime;

    int fd_;
    std::weak_ptr<Runtime> runtime_;
};

class BaseAcceptor : public Handle {
    public:
    virtual ~BaseAcceptor();

    protected:
    friend class Runtime;

    virtual void do_accept() = 0;
};

class BaseSocket : public Handle {
    public:
    virtual ~BaseSocket();

    protected:
    friend class Runtime;

    virtual void do_read() = 0;
    virtual void do_write() = 0;
};

}