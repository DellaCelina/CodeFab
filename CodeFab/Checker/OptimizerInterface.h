#pragma once

#include "../Assembler/SyntaxTree.h"

// TODO.md #10: Checker(의미 오류 검사)와 Optimizer(상수 폴딩 등 실행 전 최적화)의
// 책임을 분리하기로 팀에서 결정했다. 이 인터페이스는 계약(선언)일 뿐이며, 기존
// Checker.cpp의 foldConstantIfPossible()에 있던 ConstantFolder 로직을 이 인터페이스의
// 구현체(예: Checker/Optimizer.h/.cpp)로 옮기는 작업은 Checker 담당자의 몫이다 -
// ImplementTodo.md 참고.
//
// - optimize(tree)는 반드시 CheckerInterface::check(tree)가 예외 없이 통과한(의미
//   오류가 없는) SyntaxTree에 대해서만 호출되어야 한다. 오류 검사와 책임이 분리되어
//   있으므로, 최적화를 적용하지 못하는 경우(예: "1 + (3 / 0)"처럼 상수처럼 보이지만
//   런타임에 0으로 나누기 오류가 나야 하는 식)는 예외를 던지지 않고 그냥 원본
//   서브트리를 그대로 둔다 - Architecture.md §6.2 참고.
// - Optimizer 구현체도 Checker와 마찬가지로 ExecuteInterface&(evaluate() 호출용)에
//   의존하는 것을 권장한다(DIP 유지) - TODO.md #10 "제안된 방향" 참고.
// - Shell 담당자는 main.cpp에서 Optimizer를 생성한 뒤, checker_.check(tree)가
//   성공한 직후 executor_.execute(tree) 이전에 optimizer_.optimize(tree)를 호출하도록
//   RunPromptShell/FileRunMode/DebugMode 파이프라인에 끼워 넣는다.
class OptimizerInterface {
public:
    virtual ~OptimizerInterface() = default;

    virtual void optimize(SyntaxTree& tree) = 0;
};
