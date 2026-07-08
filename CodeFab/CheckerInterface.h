#pragma once

#include <stdexcept>
#include <string>

#include "SyntaxTree.h"

// Checker가 의미 오류(변수 중복 선언, 자기 참조 등)를 발견하면 던지는 예외.
// 오류가 발생한 소스 줄 번호(line)를 함께 보관한다. ExecuteInterface.h의
// ExecutorError와 같은 모양: 자신을 던지는 인터페이스 헤더에 함께 정의해서,
// 그 인터페이스의 구현체가 어떤 예외를 던질 수 있는지 한곳에서 알 수 있게 한다.
class CheckerError : public std::runtime_error {
public:
    CheckerError(int line, const std::string& message)
        : std::runtime_error(message), line_(line) {
    }

    int line() const { return line_; }

private:
    int line_;
};

// 문법 트리를 실행 전 검사한다. (담당: Checker)
// 의미 오류(변수 중복 선언, 자기 참조 등) 발견 시 상세 메시지가 필요한 경우
// CheckerError(line, message) 를 throw 할 수 있다.
// throw 없이 단순 실패만 알릴 경우 false 를 반환해도 되며,
// Shell은 두 가지 방식 모두를 처리한다.
class CheckerInterface {
public:
    virtual ~CheckerInterface() = default;

    virtual bool check(SyntaxTree& tree) = 0;
};
