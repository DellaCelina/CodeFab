#pragma once

#include <string>
#include <vector>

#include "AssemblerInterface.h"
#include "SourceReaderInterface.h"

// Assembler는 import 구문(3일차 확장, Architecture.md §7)을 처리하기 위해
// SourceReaderInterface(파일 경로 -> 토큰화까지 완료된 토큰 목록)에 의존한다.
// 파일을 읽고 토큰화하는 두 단계는 SourceReaderInterface 구현체(FileSourceReader)
// 안에 캡슐화되어 있어서, Assembler는 Tokenizer를 직접 알 필요가 없다.
class Assembler : public AssemblerInterface {
public:
    explicit Assembler(SourceReaderInterface& sourceReader);

    SyntaxTree assemble(const std::vector<Token>& tokens) override;
    std::vector<Statement*> assembleAll(const std::vector<Token>& tokens, SyntaxTree& tree) override;

private:
    SourceReaderInterface& sourceReader_;

    // 순환 import 검출용. import 구문을 파싱하는 동안 재귀적으로 assemble()을
    // 호출하는 내부 Parser들이 이 벡터를 공유해, 지금 "해석 중"인 파일 경로를
    // 스택처럼 쌓아 올린다 - Architecture.md §7.2 참고.
    std::vector<std::string> importStack_;
};
