#include <cstring>

#include "spinet/core/result.h"

using namespace spinet;

Result Result::ok() {
    return Result {};
}

Result Result::system_error(int ec) {
    return Result { ec };
}

Result Result::custom_error(const std::string& msg) {
    return Result { msg };
}

Result::Result()
: err_ {} {
}

Result::Result(int ec)
: err_ { ec } {
}

Result::Result(const std::string& msg)
: err_ { msg } {
}

Result::operator bool() {
    return !err_.has_value();
}

bool Result::operator!() {
    return err_.has_value();
}

bool Result::is_custom_error() {
    if (err_ && err_.value().index() == 1) {
        return false;
    } else {
        return true;
    }
}

std::string Result::to_string() {
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