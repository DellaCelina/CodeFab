#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ArrayRuntime.h"
#include "ClassRuntime.h"
#include "Environment.h"
#include "ExecuteInterface.h"
#include "ModuleRuntime.h"
#include "../Assembler/SyntaxTree.h"
#include "Value.h"

// SyntaxTree를 Visitor 패턴으로 실행한다. visit() 누락은 컴파일 오류로 잡힌다.
// 클래스/모듈/배열 관련 책임은 ClassRuntime/ModuleRuntime/ArrayRuntime에 위임한다.
class Executor : public ExecuteInterface, public SyntaxNodeVisitor {
    friend class ClassRuntime;
    friend class ModuleRuntime;

public:
    // out 기본값은 std::cout. 테스트에서는 ostringstream으로 교체해 출력을 캡처한다.
    explicit Executor(std::ostream& out = std::cout);

    void execute(SyntaxTree& tree) override;
    void execute(Statement* stmt);

    Value evaluate(Expression* expr) override;

    const Environment& environment() const override;

    // 디버그 모드 전용. Statement 실행 직전에 호출되며, depth는 최상위=1 기준의
    // 재귀 깊이다. RunPromptShell/FileRunMode는 이 훅을 설정하지 않는다.
    using StatementHook = std::function<void(Statement*, int depth)>;
    void setStatementHook(StatementHook hook);

    // SyntaxNodeVisitor: 노드 하나당 visit() 하나. Statement류는 부수효과만
    // 일으키고 끝나고, Expression류는 결과를 lastValue_에 담아둔다.
    void visit(IdentifierExpression& node) override;
    void visit(PrintStatement& node) override;
    void visit(ExpressionStatement& node) override;
    void visit(DeclareStatement& node) override;
    void visit(BlockStatement& node) override;
    void visit(IfStatement& node) override;
    void visit(ForStatement& node) override;
    void visit(NumberExpression& node) override;
    void visit(StringExpression& node) override;
    void visit(BooleanExpression& node) override;
    void visit(AddExpression& node) override;
    void visit(MultExpression& node) override;
    void visit(SubExpression& node) override;
    void visit(DivideExpression& node) override;
    void visit(ModExpression& node) override;
    void visit(AndExpression& node) override;
    void visit(OrExpression& node) override;
    void visit(EqualExpression& node) override;
    void visit(NotEqualExpression& node) override;
    void visit(LessExpression& node) override;
    void visit(LessEqualExpression& node) override;
    void visit(GreaterExpression& node) override;
    void visit(GreaterEqualExpression& node) override;
    void visit(AssignExpression& node) override;
    void visit(NegativeExpression& node) override;
    void visit(NotExpression& node) override;
    void visit(CallExpression& node) override;
    void visit(FieldAccessExpression& node) override;
    void visit(ThisExpression& node) override;
    void visit(SuperExpression& node) override;
    void visit(ArrayExpression& node) override;
    void visit(IndexExpression& node) override;
    void visit(InstanceOfExpression& node) override;
    void visit(FunctionDeclareStatement& node) override;
    // MethodDeclareStatement는 클래스 바디 전용 선언이라 execute()/accept()로
    // 직접 방문되지 않는다(instantiate/callMethod가 findMethod()로 찾아 바로
    // invoke()에 넘긴다) - Checker의 동일 노드 처리와 같은 이유.
    void visit(MethodDeclareStatement& node) override;
    void visit(ReturnStatement& node) override;
    void visit(ClassDeclareStatement& node) override;
    void visit(ImportStatement& node) override;

private:
    void requireNumberOperands(const Value& left, const Value& right, const char* op, int line) const;

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

    // callee가 FieldAccessExpression인 CallExpression(메서드 호출) 처리 - 대상이
    // Super/모듈/인스턴스 중 무엇인지만 가려서 해당 Runtime에 위임한다.
    Value callMethod(FieldAccessExpression* fieldAccess, const std::vector<Expression*>& argExprs);

    std::ostream& out_;
    Environment environment_;
    StatementHook statementHook_;
    int statementDepth_ = 0;
    Value lastValue_;  // visit(Expression&)이 결과를 여기 담아두고 evaluate()가 꺼내 간다.

    ClassRuntime classRuntime_;
    ModuleRuntime moduleRuntime_;
    ArrayRuntime arrayRuntime_;
};
