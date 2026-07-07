#pragma once

#include <stdexcept>
#include <string>

// Prompt Shell 파이프라인의 각 Unit에서 발생하는 오류의 공통 베이스.
// 오류가 발생한 소스 줄 번호(line)를 함께 보관한다.
class CodeFabError : public std::runtime_error {
public:
    CodeFabError(int line, const std::string& message)
        : std::runtime_error(message), line_(line) {
    }

    int Line() const { return line_; }

private:
    int line_;
};

// TokenizeInterface / AssemblerInterface: 문법(구문) 오류
class AssemblyError : public CodeFabError {
public:
    using CodeFabError::CodeFabError;
};

// TokenizeInterface: 소스가 아직 완결되지 않음(괄호/문자열이 안 닫힘).
// 오류가 아니라 Shell이 입력을 더 받아야 한다는 신호로 사용한다.
class IncompleteInputError : public CodeFabError {
public:
    using CodeFabError::CodeFabError;
};

// CheckerInterface: 의미 오류 (변수 중복 선언, 자기 참조 등)
class CheckError : public CodeFabError {
public:
    using CodeFabError::CodeFabError;
};

// ExecuteInterface: 런타임 오류 (타입 불일치, 미정의 변수, 0으로 나누기 등)
// std::runtime_error 와 이름 충돌을 피하기 위해 RuntimeCodeFabError 로 명명한다.
class RuntimeCodeFabError : public CodeFabError {
public:
    using CodeFabError::CodeFabError;
};
