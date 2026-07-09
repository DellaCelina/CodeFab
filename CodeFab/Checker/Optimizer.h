#pragma once
#include "OptimizerInterface.h"
#include "../Executor/ExecuteInterface.h"

// TODO.md #10: Checker.cpp의 foldConstantIfPossible()에 있던 ConstantFolder 로직을
// 이 클래스로 옮겼다. Checker(의미 오류 검사)와 책임이 분리되어, optimize()는
// check()가 true를 반환한 트리에 대해서만 호출돼야 한다(OptimizerInterface.h 참고).
//
// Executor/Checker와 동일하게 Visitor 패턴(SyntaxNodeVisitor, TODO.md #11)으로
// 순회한다. accept()가 매칭되는 visit() 오버라이드를 호출하므로, 새 노드 타입의
// visit()을 빼먹으면 (예전의 dynamic_cast 체인처럼 조용히 무시되는 게 아니라)
// 컴파일 에러가 난다.
class Optimizer : public OptimizerInterface, public SyntaxNodeVisitor {
public:
    explicit Optimizer(ExecuteInterface& executor) : executor_(executor) {}

    void optimize(SyntaxTree& tree) override;

    // SyntaxNodeVisitor: 노드 하나당 visit() 하나. Statement류는 자식을 재귀
    // 폴딩하는 부수효과만 일으키고 끝나고, Expression류는 폴딩된(또는 원래의)
    // 자기 자신을 lastFolded_에 담아둔다 - Executor::lastValue_와 같은 관례
    // (visit()가 값을 반환하지 않으므로 결과를 멤버에 담아두고 꺼내 쓴다).
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
    // MethodDeclareStatement는 클래스 바디 전용 선언이라 accept()로 직접
    // 방문되지 않는다(visit(ClassDeclareStatement&)가 각 메서드의 body를 바로
    // 접는다) - Checker/Executor의 동일 노드 처리와 같은 이유.
    void visit(MethodDeclareStatement& node) override;
    void visit(ReturnStatement& node) override;
    void visit(ClassDeclareStatement& node) override;
    void visit(ImportStatement& node) override;

private:
    ExecuteInterface& executor_;
    SyntaxTree* tree_ = nullptr; // optimize() 호출 중에만 유효 - 새 리터럴 노드의 소유권 등록용

    Expression* lastFolded_ = nullptr; // visit(Expression&) 오버라이드가 결과를 여기 담아둔다

    // accept()/visit() 이중 디스패치 진입점. nullptr은 호출부가 미리 걸러낸다
    // (IfStatement::elseBranch, ReturnStatement::value처럼 nullable한 필드가
    // 대상일 때) - Executor의 execute()/evaluate()와 동일한 관례.
    void foldStatement(Statement* stmt);

    // expr을 폴딩한 뒤 그 자리에 써야 할 노드를 반환한다. BinaryExpression::left/right,
    // UnaryExpression::operand처럼 non-const 필드를 가진 호출부만 반환값을 실제로
    // 대입해 트리를 바꿀 수 있다 - 나머지 필드(PrintStatement::expr 등)는 여전히
    // const라 반환값을 버리고 부작용(자식 폴딩)만 취한다.
    Expression* foldExpression(Expression* expr);

    // BinaryExpression 하위 13개 타입이 전부 "양쪽을 먼저 접고, 둘 다 리터럴이면
    // evaluate()로 값을 구해 리터럴로 치환"만 동일하게 하므로 공유 헬퍼로 둔다.
    // 결과는 lastFolded_에 담긴다.
    void foldBinary(BinaryExpression& bin);

    // NegativeExpression/NotExpression이 공유하는 단항 버전. 결과는 lastFolded_에 담긴다.
    void foldUnary(UnaryExpression& un);

    Expression* replaceWithLiteral(const Value& value, Expression* original);
};
