#pragma once

#include <string>
#include <vector>

#include "AssemblerInterface.h"
#include "SourceReaderInterface.h"

class Assembler : public AssemblerInterface {
public:
    explicit Assembler(SourceReaderInterface& sourceReader);

    SyntaxTree assemble(const std::vector<Token>& tokens) override;

private:
    SourceReaderInterface& sourceReader_;

    std::vector<std::string> importStack_; // 순환 import 검출용
};
