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
    // sourceLines는 원본 파일을 줄 단위로 나눈 것(1번째 줄 = sourceLines[0])이다.
    // 정지할 때 "[DEBUG] N번째 줄에서 정지" 다음에 그 줄의 실제 소스를
    // "-> " 접두사로 이어서 보여주는 데 쓴다. 비워두면(기본값) 이 줄 표시를
    // 생략한다 - DebugMode가 아닌 다른 방식으로 Debugger를 쓰는 경우(예: 파일
    // 내용 없이 직접 만든 SyntaxTree로 하는 단위 테스트)까지 소스 텍스트를
    // 강제로 요구하지 않기 위함이다.
    Debugger(const ExecuteInterface& executor, std::istream& in, std::ostream& out,
             std::vector<std::string> sourceLines = {});

    // Executor::setStatementHook에 그대로 넘길 수 있는 콜백.
    void onStatement(Statement* stmt, int depth);

private:
    enum class Mode { Step, Next, Continue };

    bool shouldStop(Statement* stmt, int depth) const;
    void printWatches() const;
    void printBreakpoints() const;
    void printInspect() const;
    void printCurrentSourceLine(int line) const;
    void promptAndHandleCommand(Statement* stmt, int depth);

    const ExecuteInterface& executor_;
    std::istream& in_;
    std::ostream& out_;
    std::vector<std::string> sourceLines_;
    std::set<int> breakpoints_;
    std::vector<std::string> watches_;
    Mode mode_ = Mode::Step;
    // "next"를 입력한 시점에 멈춰 있던 depth. 다음 statement의 depth가 이
    // 값보다 크면(현재 줄의 하위) 건너뛰고, 이 값 이하로 돌아오면 멈춘다.
    int nextTargetDepth_ = 0;
};
