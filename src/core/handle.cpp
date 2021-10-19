#include "unistd.h"

#include "runtime.h"
#include "spinet/core/handle.h"

using namespace spinet;

Handle::~Handle() {
}

void Handle::close() {
    if (auto runtime = runtime_.lock()) {
        runtime->deregister_handle(this);
    }
    ::close(fd_);
}

BaseAcceptor::~BaseAcceptor() {
}

BaseSocket::~BaseSocket() {
}