#pragma once

#include <memory>
#include <utility>

#include "../Assembler/SyntaxTree.h"
#include "Value.h"

class Executor;
struct ArrayValue;

// 고정 크기 배열의 생성/인덱싱을 전담한다(Executor의 God Object 분리 -
// 전체리팩토링리스트 #3). Executor의 private 멤버(evaluate)만 필요하지만,
// 다른 Runtime과 구성을 맞추기 위해 동일하게 friend로 접근한다.
class ArrayRuntime {
public:
    explicit ArrayRuntime(Executor& executor);

    // ArrayExpression 평가: sizeExpr을 평가해 그 크기만큼 Nil로 채운 배열을 만든다.
    Value create(Expression* sizeExpr);

    // collectionExpr/indexExpr을 평가해 (배열, 검증된 인덱스)를 반환한다. 배열이
    // 아니거나 인덱스가 숫자가 아니거나 범위를 벗어나면 ExecutorError. 배열
    // 자체(shared_ptr)를 값으로 반환해서, 호출부가 collectionExpr을 다시
    // 평가하는 동안에만 살아있는 임시 Value에 원소 참조가 매달리는 일이 없게 한다.
    std::pair<std::shared_ptr<ArrayValue>, size_t> resolveIndex(Expression* collectionExpr, Expression* indexExpr);

private:
    Executor& executor_;
};
