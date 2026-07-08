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

// Tokenizer가 소스가 아직 완결되지 않았음을 알릴 때(괄호/문자열이 안 닫힘) 던지는
// 예외. 오류가 아니라 Shell이 입력을 더 받아야 한다는 신호로 사용한다.
class IncompleteInputError : public std::exception {
public:
    explicit IncompleteInputError(const std::string& msg) : msg_(msg) {}

    const char* what() const override {
        return msg_.c_str();
    }

private:
    std::string msg_;
};

// 소스코드를 의미 있는 최소 단위(Token)로 분해한다. (담당: Tokenizer)
// 인식 불가능한 문자를 만나면 AssemblyError 를 throw 한다.
// 소스가 아직 완결되지 않은 경우(괄호/문자열이 안 닫힘)에는 IncompleteInputError 를 throw 한다.
// RunPromptShell은 이 신호로 "입력을 더 받아야 하는지"를 판단한다.
class TokenizeInterface {
public:
    virtual ~TokenizeInterface() = default;

    virtual std::vector<Token> tokenize(const std::string& source) = 0;
};
