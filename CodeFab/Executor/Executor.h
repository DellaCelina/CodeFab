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

// Executes a SyntaxTree via recursive DFS.
//
// Dispatch is done through the Visitor pattern (SyntaxNodeVisitor, TODO.md
// #11): each node's accept() calls back into the matching visit() override,
// so the compiler enforces that every concrete node type is handled - unlike
// a dynamic_cast/type_index chain, forgetting a node type is a compile error
// instead of a silent runtime fallback.
//
// 클래스 인스턴스화/메서드 탐색(ClassRuntime), import 실행/모듈 멤버 호출
// (ModuleRuntime), 배열 생성/인덱싱(ArrayRuntime)은 각각 서로 다른 이유로
// 바뀌는 책임이라 별도 협력 객체로 분리했다(전체리팩토링리스트 #3, God Object
// 분리). Executor는 이 셋에게 접근을 허용하는 friend이고, visit()들은 대부분
// "어떤 Runtime에 위임할지 결정"만 하는 얇은 라우팅 역할만 한다.
class Executor : public ExecuteInterface, public SyntaxNodeVisitor {
    friend class ClassRuntime;
    friend class ModuleRuntime;

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
