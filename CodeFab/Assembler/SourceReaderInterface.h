#pragma once

#include <string>
#include <vector>

#include "../Tokenizer/Token.h"

// 소스 파일을 읽어 토큰 목록으로 돌려주는 인터페이스.
// Assembler가 파일 시스템/Tokenizer에 직접 의존하지 않도록 분리한다.
class SourceReaderInterface {
public:
    virtual ~SourceReaderInterface() = default;

    virtual std::vector<Token> read(const std::string& path) = 0;
};
