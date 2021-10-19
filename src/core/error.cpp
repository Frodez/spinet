#include <cstring>

#include "spinet/core/error.h"

using namespace spinet;

Error Error::ok() {
    return Error {};
}

Error Error::system_error(int ec) {
    return Error { ec };
}

Error Error::custom_error(const std::string& msg) {
    return Error { msg };
}

Error::Error()
: err_ {} {
}

Error::Error(int ec)
: err_ { ec } {
}

Error::Error(const std::string& msg)
: err_ { msg } {
}

Error::operator bool() {
    return err_.has_value();
}

bool Error::operator!() {
    return !err_.has_value();
}

bool Error::is_custom_error() {
    if (err_ && err_.value().index() == 1) {
        return false;
    } else {
        return true;
    }
}

std::string Error::to_string() {
    if (!err_) {
        return {};
    } else {
        auto& v = err_.value();
        if (v.index() == 0) {
            return std::strerror(std::get<0>(v));
        } else {
            return std::get<1>(v);
        }
    }
}