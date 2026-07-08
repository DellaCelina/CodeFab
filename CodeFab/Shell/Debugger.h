#pragma once

#include <istream>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "../Assembler/SyntaxTree.h"
#include "../Executor/ExecuteInterface.h"

// 공장 제어 쉘의 디버그 모드용 인터랙티브 디버거(Architecture.md §9.3,
// Implement.md §5 "할 일 3"). Executor::setStatementHook에 등록해서 넘기는
// onStatement()가 Statement 하나가 실제로 실행되기 직전마다 호출된다 -
// 현재 모드(step/next/continue)와 브레이크포인트에 따라 멈출지 그냥
// 지나갈지 정하고, 멈추면 watch 값을 출력한 뒤 in_에서 명령을 읽어 처리한다.
//
// Executor가 실제로 제공하는 것만 사용한다(전부 실제 구현체, Mock 없음):
// - ExecuteInterface::environment() -> watch/inspect가 값을 읽는다.
// - Statement::getLine()/containsLine() -> 브레이크포인트 매칭.
// - Executor::StatementHook의 depth 인자 -> "next"(현재 줄의 하위
//   statement는 건너뛰고, 같거나 더 얕은 깊이로 돌아왔을 때만 멈추는
//   step-over)를 정확히 구현하는 데 쓰인다.
class Debugger {
public:
    Debugger(const ExecuteInterface& executor, std::istream& in, std::ostream& out);

    // Executor::setStatementHook에 그대로 넘길 수 있는 콜백.
    void onStatement(Statement* stmt, int depth);

private:
    enum class Mode { Step, Next, Continue };

    bool shouldStop(Statement* stmt, int depth) const;
    void printWatches() const;
    void printBreakpoints() const;
    void printInspect() const;
    void promptAndHandleCommand(Statement* stmt, int depth);

    const ExecuteInterface& executor_;
    std::istream& in_;
    std::ostream& out_;
    std::set<int> breakpoints_;
    std::vector<std::string> watches_;
    Mode mode_ = Mode::Step;
    // "next"를 입력한 시점에 멈춰 있던 depth. 다음 statement의 depth가 이
    // 값보다 크면(현재 줄의 하위) 건너뛰고, 이 값 이하로 돌아오면 멈춘다.
    int nextTargetDepth_ = 0;
};
