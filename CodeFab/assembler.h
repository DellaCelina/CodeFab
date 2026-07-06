#pragma once

#include "assembler_interface.h"

class Assembler : public AssemblerInterface {
public:
    std::unique_ptr<SyntaxTree> assemble(const std::vector<Token> tokens) override;
};
