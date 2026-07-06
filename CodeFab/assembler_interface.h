#pragma once

#include <memory>
#include <vector>

#include "syntax_tree.h"

struct AssemblerInterface {
    virtual std::unique_ptr<SyntaxTree> assemble(const std::vector<Token> tokens) = 0;
};
