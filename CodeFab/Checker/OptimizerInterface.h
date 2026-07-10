#pragma once

#include "../Assembler/SyntaxTree.h"

// check()가 예외 없이 통과한 SyntaxTree에 대해서만 optimize()를 호출해야 한다.
// 폴딩 불가능한 서브트리(예: "1 + (3 / 0)")는 예외 없이 원본을 그대로 둔다.
class OptimizerInterface {
public:
    virtual ~OptimizerInterface() = default;

    virtual void optimize(SyntaxTree& tree) = 0;
};
