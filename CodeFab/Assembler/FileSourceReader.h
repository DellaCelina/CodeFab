#pragma once

#include "SourceReaderInterface.h"
#include "../Tokenizer/TokenizeInterface.h"

// SourceReaderInterface의 실제 파일 시스템 구현체. 생성자로 받은
// TokenizeInterface를 내부에 들고 있다가, read()에서 파일을 읽자마자 그
// 자리에서 토큰화까지 마쳐서 돌려준다 - Assembler는 Tokenizer를 몰라도 된다.
// 운영 환경(main.cpp)에서 사용하고, 테스트에서는 대신 인메모리 Fake를 주입한다.
class FileSourceReader : public SourceReaderInterface {
public:
    explicit FileSourceReader(TokenizeInterface& tokenizer);

    std::vector<Token> read(const std::string& path) override;

private:
    TokenizeInterface& tokenizer_;
};
