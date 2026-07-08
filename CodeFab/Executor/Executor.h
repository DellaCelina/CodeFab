#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

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
    // 호출된다. 두 번째 인자는 현재 실행 깊이(최상위 statement = 1, 그 statement가
    // Block/If/For의 바디이거나 함수/메서드 호출 안이면 그보다 큰 값)다 - 디버그
    // 모드의 "next"(현재 줄 안의 하위 statement는 건너뛰고, 같거나 더 얕은 깊이로
    // 돌아왔을 때만 멈추는 step-over) 명령을 구현하려면 depth 정보가 반드시
    // 필요하다(Implement.md §5 "할 일 3"이 이 필요성을 미리 언급해뒀다). depth는
    // execute(Statement*)가 재귀 호출될 때마다 증가/감소하는 내부 카운터를 그대로
    // 넘겨준다. RunPromptShell/FileRunMode는 이 훅을 설정하지 않으므로 기존 동작에
    // 영향이 없다. ExecuteInterface에는 포함하지 않는다 - 디버그 모드만 이 구체
    // 타입(Executor)에 직접 의존해서 쓴다 (Architecture.md §9.3 참고).
    using StatementHook = std::function<void(Statement*, int depth)>;
    void setStatementHook(StatementHook hook);

private:
    void registerDefaultHandlers();
    void requireNumberOperands(const Value& left, const Value& right, const char* op) const;

    // 함수/메서드 호출의 공용 절차: 새 스코프를 push하고 (this가 있으면 먼저
    // bind한 뒤) 파라미터를 bind, body를 실행, ReturnStatement가 던지는
    // ReturnSignal을 잡아 반환값으로 변환한다. FunctionDeclareStatement(this
    // 없음)와 MethodDeclareStatement(this 있음, §클래스)가 이 헬퍼 하나를
    // 공유한다.
    Value invoke(const Token& name, const std::vector<Token>& params, const std::vector<Statement*>& body,
        const std::vector<Value>& args, std::optional<Value> boundThis = std::nullopt);

    // 최상위 함수 호출 (this 없음).
    Value callFunction(const FunctionDeclareStatement* decl, const std::vector<Value>& args);

    // 클래스 메서드 호출 (this 있음) - invoke()를 그대로 재사용한다.
    Value callMethodDecl(const MethodDeclareStatement* method, const std::vector<Value>& args, Value boundThis);

    // klass를 인스턴스화한다: 필드 저장소를 만들고, init 메서드가 있으면
    // 호출한다(반환값은 버리고 항상 새 인스턴스를 반환).
    Value instantiate(const ClassDeclareStatement* klass, const std::vector<Value>& args);

    // callee가 FieldAccessExpression인 CallExpression(메서드 호출) 처리.
    Value callMethod(FieldAccessExpression* fieldAccess, const std::vector<Expression*>& argExprs);

    // collectionExpr/indexExpr을 평가해 (배열, 검증된 인덱스)를 반환한다. 배열이
    // 아니거나 인덱스가 숫자가 아니거나 범위를 벗어나면 ExecutorError. 배열
    // 자체(shared_ptr)를 값으로 반환해서, 호출부가 collectionExpr을 다시
    // 평가하는 동안에만 살아있는 임시 Value에 원소 참조가 매달리는 일이 없게 한다.
    std::pair<std::shared_ptr<ArrayValue>, size_t> resolveArrayIndex(Expression* collectionExpr, Expression* indexExpr);

    std::ostream& out_;
    Environment environment_;
    StatementHook statementHook_;
    int statementDepth_ = 0;
    std::unordered_map<std::type_index, std::function<void(Statement*)>> statementHandlers_;
    std::unordered_map<std::type_index, std::function<Value(Expression*)>> expressionHandlers_;
};
