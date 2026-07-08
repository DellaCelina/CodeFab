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

    int line() const { return line_; }

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
