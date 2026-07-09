#pragma once

#include <vector>
#include <string>
#include <stdexcept>
#include <format>

#include "SyntaxTree.h"
#include "../Tokenizer/Token.h"

class AssemblerError : public std::exception {
public:
    template <typename ...Args>
    AssemblerError(std::format_string<Args...> fmt, Args&&... args) {
        msg_ = std::format(fmt, std::forward<Args>(args)...);
    }

    AssemblerError(const std::string& msg) : msg_(msg) {}

    const char* what() const override {
        return msg_.c_str();
    }
private:
    std::string msg_;
};

struct AssemblerInterface {
    virtual SyntaxTree assemble(const std::vector<Token>& tokens) = 0;
};
