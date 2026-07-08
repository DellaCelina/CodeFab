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

    // 표현식 하나만 평가해 값을 반환한다. RunPromptShell의 파이프라인에서는
    // 쓰이지 않지만, Checker의 상수 연산 최적화(Architecture.md §6.2)가 산술
    // 규칙을 다시 구현하지 않고 이 메서드를 그대로 호출해서 리터럴만으로 이뤄진
    // 서브트리의 값을 계산한다.
    virtual Value evaluate(Expression* expr) = 0;

    // 현재 변수 저장소를 읽기 전용으로 노출한다. 디버그 모드(Shell, Architecture.md
    // §9.3)가 statement 실행이 멈춘 시점마다 watch 대상 변수를 조회하는 데 쓴다.
    virtual const Environment& environment() const = 0;
};
