#pragma once

#include "AssemblerInterface.h"
#include "SourceReaderInterface.h"
#include "../Tokenizer/TokenizeInterface.h"

// Assembler는 import 구문(3일차 확장, Architecture.md §7)을 처리하기 위해
// TokenizeInterface(대상 파일 내용을 다시 토큰화)와 SourceReaderInterface(파일
// 내용 읽기)에 의존한다. 지금 문법(3일차 import 이전)은 이 두 의존성을 아직
// 쓰지 않지만, 여러 사람이 병렬로 개발할 수 있도록 생성자 계약을 먼저
// 확정해둔다 - 구현 방법은 Implement.md의 Assembler 담당자 안내 참고.
class Assembler : public AssemblerInterface {
public:
    Assembler(TokenizeInterface& tokenizer, SourceReaderInterface& sourceReader);

    SyntaxTree assemble(const std::vector<Token>& tokens) override;

private:
    TokenizeInterface& tokenizer_;
    SourceReaderInterface& sourceReader_;
};
