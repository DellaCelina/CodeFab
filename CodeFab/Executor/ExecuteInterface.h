#pragma once

#include <format>
#include <stdexcept>
#include <string>

#include "../Assembler/SyntaxTree.h"
#include "Environment.h"
#include "Value.h"

// 실행 중 발생하는 런타임 오류(타입 불일치, 미정의 변수, 0으로 나누기 등)를
// 나타낸다. AssemblerInterface.h의 AssemblerError와 같은 모양: std::exception을
// 직접 상속하고, std::format 기반 생성자로 메시지를 조립한다.
class ExecutorError : public std::exception {
public:
    template <typename... Args>
    ExecutorError(std::format_string<Args...> fmt, Args&&... args) {
        msg_ = std::format(fmt, std::forward<Args>(args)...);
    }

    explicit ExecutorError(const std::string& msg) : msg_(msg) {}

    const char* what() const override {
        return msg_.c_str();
    }

private:
    std::string msg_;
};

// 문법 트리를 실제로 실행한다. (담당: Executor)
// 실행 중 발생하는 런타임 오류(타입 불일치, 미정의 변수, 0으로 나누기 등)는
// ExecutorError 를 throw 한다.
class ExecuteInterface {
public:
    virtual ~ExecuteInterface() = default;

    virtual void execute(SyntaxTree& tree) = 0;

    // Optimizer가 상수 폴딩 시 산술 규칙을 직접 구현하지 않고 이 메서드로 계산한다.
    virtual Value evaluate(Expression* expr) = 0;

    virtual const Environment& environment() const = 0;
};
