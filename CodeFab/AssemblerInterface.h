#pragma once

#include <vector>

#include "SyntaxTree.h"
#include "Token.h"

// Token List를 가공하여 실행 가능한 문법 트리(SyntaxTree)로 조립한다. (담당: Assembler)
// 문법 오류 발견 시 AssemblyError 를 throw 한다.
class AssemblerInterface {
public:
    virtual ~AssemblerInterface() = default;

    virtual SyntaxTree assemble(const std::vector<Token>& tokens) = 0;
};
