#pragma once

#include <stdexcept>
#include <string>

inline int expectInt(const std::string& str, const std::string& msg) {
    try {
        return std::stoi(str);
    } catch (std::exception& e) {
        throw std::runtime_error { msg };
    }
}