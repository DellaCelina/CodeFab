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

    // assemble()과 달리 토큰을 끝까지 소비해 최상위 statement 여러 개를 전부
    // 파싱한다(REPL에서 한 제출에 여러 문장이 들어온 경우를 위해 - RunPromptShell
    // 참고). 반환되는 Statement*들은 모두 tree가 소유하므로, 호출부는 tree를
    // 각 statement 실행이 끝날 때까지 살려두어야 한다.
    virtual std::vector<Statement*> assembleAll(const std::vector<Token>& tokens, SyntaxTree& tree) = 0;
};
