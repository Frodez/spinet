#pragma once

#include <optional>
#include <string>
#include <variant>

namespace spinet {

class Result {
    public:
    static Result ok();
    static Result system_error(int ec);
    static Result custom_error(const std::string& msg);

    operator bool();
    bool operator!();
    bool is_custom_error();
    std::string to_string();

    private:
    explicit Result();
    explicit Result(int ec);
    explicit Result(const std::string& msg);

    std::optional<std::variant<int, std::string>> err_;
};

}