#pragma once
#include "OptimizerInterface.h"
#include "../Executor/ExecuteInterface.h"

// TODO.md #10: Checker.cpp의 foldConstantIfPossible()에 있던 ConstantFolder 로직을
// 이 클래스로 옮겼다. Checker(의미 오류 검사)와 책임이 분리되어, optimize()는
// check()가 true를 반환한 트리에 대해서만 호출돼야 한다(OptimizerInterface.h 참고).
class Optimizer : public OptimizerInterface {
public:
    explicit Optimizer(ExecuteInterface& executor) : executor_(executor) {}

    void optimize(SyntaxTree& tree) override;

private:
    ExecuteInterface& executor_;
    SyntaxTree* tree_ = nullptr; // optimize() 호출 중에만 유효 - 새 리터럴 노드의 소유권 등록용

    void foldStatement(Statement* stmt);

    // expr을 폴딩한 뒤 그 자리에 써야 할 노드를 반환한다. BinaryExpression::left/right,
    // UnaryExpression::operand처럼 non-const 필드를 가진 호출부만 반환값을 실제로
    // 대입해 트리를 바꿀 수 있다 - 나머지 필드(PrintStatement::expr 등)는 여전히
    // const라 반환값을 버리고 부작용(자식 폴딩)만 취한다.
    Expression* foldExpression(Expression* expr);

    Expression* replaceWithLiteral(const Value& value, Expression* original);
};
