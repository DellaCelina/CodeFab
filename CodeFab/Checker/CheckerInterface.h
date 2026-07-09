#pragma once

#include <format>
#include <stdexcept>
#include <string>

#include "../Assembler/SyntaxTree.h"

// Checker가 의미 오류(변수 중복 선언, 자기 참조 등)를 발견하면 던지는 예외.
// ExecuteInterface.h의 ExecutorError와 같은 모양: std::exception을 직접
// 상속하고, std::format 기반 생성자로 메시지를 조립한다. 자신을 던지는
// 인터페이스 헤더에 함께 정의해서, 그 인터페이스의 구현체가 어떤 예외를 던질 수
// 있는지 한곳에서 알 수 있게 한다. 줄 번호가 필요하면 호출부가 메시지에 직접
// 담는다 (예: CheckerError("[{}번째 줄] {}", line, ...)).
class CheckerError : public std::exception {
public:
    template <typename... Args>
    CheckerError(std::format_string<Args...> fmt, Args&&... args) {
        msg_ = std::format(fmt, std::forward<Args>(args)...);
    }

    explicit CheckerError(const std::string& msg) : msg_(msg) {}

    const char* what() const override {
        return msg_.c_str();
    }

private:
    std::string msg_;
};

// 문법 트리를 실행 전 검사한다. (담당: Checker)
// 의미 오류(변수 중복 선언, 자기 참조 등) 발견 시 CheckerError를 throw한다.
// 예외 없이 반환하면 검사를 통과한 것이다.
class CheckerInterface {
public:
    virtual ~CheckerInterface() = default;

    virtual void check(SyntaxTree& tree) = 0;
};
