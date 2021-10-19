#pragma once

#include <optional>
#include <string>
#include <variant>

namespace spinet {

class Error {
    public:
    static Error ok();
    static Error system_error(int ec);
    static Error custom_error(const std::string& msg);

    operator bool();
    bool operator!();
    bool is_custom_error();
    std::string to_string();

    private:
    explicit Error();
    explicit Error(int ec);
    explicit Error(const std::string& msg);

    std::optional<std::variant<int, std::string>> err_;
};

}