#pragma once

#include <string>
#include <vector>

#include "Token.h"

// 소스코드를 의미 있는 최소 단위(Token)로 분해한다. (담당: Tokenizer)
// 인식 불가능한 문자를 만나면 AssemblyError 를 throw 한다.
// 소스가 아직 완결되지 않은 경우(괄호/문자열이 안 닫힘)에는 IncompleteInputError 를 throw 한다.
// RunPromptShell은 이 신호로 "입력을 더 받아야 하는지"를 판단한다.
class TokenizeInterface {
public:
    virtual ~TokenizeInterface() = default;

    virtual std::vector<Token> Tokenize(const std::string& source) = 0;
};
