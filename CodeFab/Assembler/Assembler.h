#pragma once

#include "AssemblerInterface.h"
#include "SourceReaderInterface.h"

// Assembler는 import 구문(3일차 확장, Architecture.md §7)을 처리하기 위해
// SourceReaderInterface(파일 경로 -> 토큰화까지 완료된 토큰 목록)에 의존한다.
// 파일을 읽고 토큰화하는 두 단계는 SourceReaderInterface 구현체(FileSourceReader)
// 안에 캡슐화되어 있어서, Assembler는 Tokenizer를 직접 알 필요가 없다. 지금
// 문법(3일차 import 이전)은 이 의존성을 아직 쓰지 않지만, 여러 사람이 병렬로
// 개발할 수 있도록 생성자 계약을 먼저 확정해둔다 - 구현 방법은 Implement.md의
// Assembler 담당자 안내 참고.
class Assembler : public AssemblerInterface {
public:
    explicit Assembler(SourceReaderInterface& sourceReader);

    SyntaxTree assemble(const std::vector<Token>& tokens) override;

private:
    SourceReaderInterface& sourceReader_;
};
