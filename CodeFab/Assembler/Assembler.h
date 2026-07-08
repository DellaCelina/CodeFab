#pragma once

#include "AssemblerInterface.h"

class Assembler : public AssemblerInterface {
public:
    SyntaxTree assemble(const std::vector<Token>& tokens) override;
};
