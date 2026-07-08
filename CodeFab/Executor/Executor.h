#pragma once

#include <functional>
#include <iostream>
#include <typeindex>
#include <unordered_map>

#include "Environment.h"
#include "ExecuteInterface.h"
#include "../Assembler/SyntaxTree.h"
#include "Value.h"

// Executes a SyntaxTree via recursive DFS.
//
// Dispatch is done through a type_index -> handler table instead of a
// dynamic_cast/switch chain, so adding a node kind never requires editing
// evaluate()/execute() themselves — only registerDefaultHandlers() grows.
class Executor : public ExecuteInterface {
public:
    // `out` defaults to std::cout but can be swapped for e.g. an
    // ostringstream in tests to capture what print statements write.
    explicit Executor(std::ostream& out = std::cout);

    // ExecuteInterface: entry point, executes a whole program starting from
    // the tree's root. The root is always a Statement (a program is a
    // statement), so it's downcast once here.
    void execute(SyntaxTree& tree) override;

    // Executes a single statement node. Throws std::logic_error if no
    // handler was registered for its concrete type.
    void execute(Statement* stmt);

    // Evaluates a single expression node and returns its Value. Throws
    // std::logic_error if no handler was registered for its concrete type.
    Value evaluate(Expression* expr) override;

    // ExecuteInterface: 현재 변수 저장소를 읽기 전용으로 노출한다 (디버그 모드의
    // watch/inspect 용, Architecture.md §9.3).
    const Environment& environment() const override;

    // 디버그 모드(Shell) 전용 훅. Statement 하나를 실제로 실행하기 직전에 매번
    // 호출된다. RunPromptShell/FileRunMode는 이 훅을 설정하지 않으므로 기존
    // 동작에 영향이 없다. ExecuteInterface에는 포함하지 않는다 - 디버그 모드만
    // 이 구체 타입(Executor)에 직접 의존해서 쓴다 (Architecture.md §9.3 참고).
    using StatementHook = std::function<void(Statement*)>;
    void setStatementHook(StatementHook hook);

private:
    void registerDefaultHandlers();
    void requireNumberOperands(const Value& left, const Value& right, const char* op) const;

    std::ostream& out_;
    Environment environment_;
    StatementHook statementHook_;
    std::unordered_map<std::type_index, std::function<void(Statement*)>> statementHandlers_;
    std::unordered_map<std::type_index, std::function<Value(Expression*)>> expressionHandlers_;
};
