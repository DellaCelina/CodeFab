#pragma once

#include <functional>
#include <iostream>
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
    // `out` defaults to std::cout but can be swapped for e.g. an
    // ostringstream in tests to capture what print statements write.
    explicit Executor(std::ostream& out = std::cout);

    // Entry point: executes a whole program starting from the tree's root.
    // The root is always a Statement (a program is a statement), so it's
    // downcast once here rather than needing a SyntaxNode-level dispatch.
    void run(SyntaxTree& tree);

    // Executes a single statement node. Throws std::logic_error if no
    // handler was registered for its concrete type.
    void execute(Statement* stmt);

    // Evaluates a single expression node and returns its Value. Throws
    // std::logic_error if no handler was registered for its concrete type.
    Value evaluate(Expression* expr);

private:
    void registerDefaultHandlers();

    std::ostream& out_;
    std::unordered_map<std::type_index, std::function<void(Statement*)>> statementHandlers_;
    std::unordered_map<std::type_index, std::function<Value(Expression*)>> expressionHandlers_;
};
