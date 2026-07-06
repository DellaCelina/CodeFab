#pragma once

#include <string>
#include <vector>

#include "Token.h"

// 소스코드를 의미 있는 최소 단위(Token)로 분해한다. (담당: Tokenizer)
// 인식 불가능한 문자를 만나면 AssemblyError 를 throw 한다.
class TokenizeInterface {
public:
    virtual ~TokenizeInterface() = default;

    virtual std::vector<Token> Tokenize(const std::string& source) = 0;
};
