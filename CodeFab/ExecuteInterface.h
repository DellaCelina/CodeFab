#pragma once

#include "SyntaxTree.h"

// 문법 트리를 실제로 실행한다. (담당: Executor)
// 실행 중 발생하는 런타임 오류(타입 불일치, 미정의 변수, 0으로 나누기 등)는
// RuntimeCodeFabError 를 throw 한다.
class ExecuteInterface {
public:
    virtual ~ExecuteInterface() = default;

    virtual void execute(SyntaxTree& tree) = 0;
};
