#pragma once

#include <string>
#include <vector>

#include "Token.h"

// 소스코드를 의미 있는 최소 단위(Token)로 분해한다. (담당: Tokenizer)
// 인식 불가능한 문자를 만나면 AssemblyError 를 throw 한다.
//
// 참고: 현재 RunPromptShell은 이 인터페이스를 호출하기 전에 InputBuffer::IsInputComplete로
// "입력이 아직 불완전한지"를 별도로 판단한다. Tokenizer 구현체가 병합되면 그 판단 로직을
// 이 인터페이스로 흡수할지 검토 필요 (InputBuffer.h의 TODO 참고).
class TokenizeInterface {
public:
    virtual ~TokenizeInterface() = default;

    virtual std::vector<Token> Tokenize(const std::string& source) = 0;
};
