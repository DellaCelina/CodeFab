#pragma once

#include <functional>
#include <typeindex>
#include <unordered_map>

#include "MockSyntaxTree.h"
#include "Value.h"

// Executes a SyntaxTree via recursive DFS.
//
// Dispatch is done through a type_index -> handler table instead of a
// dynamic_cast/switch chain, so adding a node kind never requires editing
// evaluate()/execute() themselves — only registerDefaultHandlers() grows.
class Executor {
public:
    Executor();

    // Evaluates a single expression node and returns its Value. Throws
    // std::logic_error if no handler was registered for its concrete type.
    Value evaluate(Expression* expr);

private:
    void registerDefaultHandlers();

    std::unordered_map<std::type_index, std::function<Value(Expression*)>> expressionHandlers_;
};
