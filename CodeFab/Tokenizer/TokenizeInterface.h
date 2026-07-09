#pragma once

#include <format>
#include <stdexcept>
#include <string>
#include <vector>

#include "Token.h"

// Tokenizer가 인식 불가능한 문자를 만나면 던지는 예외. AssemblerInterface.h의
// AssemblerError와 같은 모양: std::exception을 직접 상속하고, std::format 기반
// 생성자로 메시지를 조립한다. 줄 번호가 필요하면 호출부가 메시지에 직접 담는다
// (예: AssemblyError("[{}번째 줄] {}", line, ...)).
class AssemblyError : public std::exception {
public:
    template <typename... Args>
    AssemblyError(std::format_string<Args...> fmt, Args&&... args) {
        msg_ = std::format(fmt, std::forward<Args>(args)...);
    }

    explicit AssemblyError(const std::string& msg) : msg_(msg) {}

    const char* what() const override {
        return msg_.c_str();
    }

private:
    std::string msg_;
};

// 소스코드를 의미 있는 최소 단위(Token)로 분해한다. (담당: Tokenizer)
// 인식 불가능한 문자 또는 종결되지 않은 문자열을 만나면 AssemblyError 를 throw 한다.
class TokenizeInterface {
public:
    virtual ~TokenizeInterface() = default;

    virtual std::vector<Token> tokenize(const std::string& source) = 0;
};
