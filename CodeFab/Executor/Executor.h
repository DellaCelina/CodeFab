#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "Environment.h"
#include "ExecuteInterface.h"
#include "../Assembler/SyntaxTree.h"
#include "Value.h"

// Executes a SyntaxTree via recursive DFS.
//
// Dispatch is done through the Visitor pattern (SyntaxNodeVisitor, TODO.md
// #11): each node's accept() calls back into the matching visit() override,
// so the compiler enforces that every concrete node type is handled - unlike
// a dynamic_cast/type_index chain, forgetting a node type is a compile error
// instead of a silent runtime fallback.
class Executor : public ExecuteInterface, public SyntaxNodeVisitor {
public:
    // `out` defaults to std::cout but can be swapped for e.g. an
    // ostringstream in tests to capture what print statements write.
    explicit Executor(std::ostream& out = std::cout);

    // ExecuteInterface: entry point, executes a whole program starting from
    // the tree's root. The root is always a Statement (a program is a
    // statement), so it's downcast once here.
    void execute(SyntaxTree& tree) override;

    // Executes a single statement node via accept()/visit() double dispatch.
    void execute(Statement* stmt);

    // Evaluates a single expression node via accept()/visit() double
    // dispatch and returns its Value. visit(Expression&) 오버라이드들이
    // 결과를 lastValue_에 담아두고, 여기서 그 값을 꺼내 온다.
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

    // klass부터 시작해 superclass 체인을 따라 올라가며 name과 일치하는 메서드를
    // 찾는다(자식 클래스부터 먼저 찾으므로 오버라이딩이 자연히 해결됨). 없으면
    // nullptr.
    MethodDeclareStatement* findMethod(const ClassDeclareStatement* klass, const std::string& name);

    // klass->superclass(IdentifierExpression*)를 실제 ClassDeclareStatement*로
    // 조회한다. superclass가 없으면 nullptr.
    const ClassDeclareStatement* resolveSuperclass(const ClassDeclareStatement* klass);

    // collectionExpr/indexExpr을 평가해 (배열, 검증된 인덱스)를 반환한다. 배열이
    // 아니거나 인덱스가 숫자가 아니거나 범위를 벗어나면 ExecutorError. 배열
    // 자체(shared_ptr)를 값으로 반환해서, 호출부가 collectionExpr을 다시
    // 평가하는 동안에만 살아있는 임시 Value에 원소 참조가 매달리는 일이 없게 한다.
    std::pair<std::shared_ptr<ArrayValue>, size_t> resolveArrayIndex(Expression* collectionExpr, Expression* indexExpr);

    std::ostream& out_;
    Environment environment_;
    StatementHook statementHook_;
    Value lastValue_;  // visit(Expression&)이 결과를 여기 담아두고 evaluate()가 꺼내 간다.
};
