#pragma once

#include <istream>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "../Assembler/SyntaxTree.h"
#include "../Executor/ExecuteInterface.h"

// setStatementHook에 등록되어 Statement 실행 직전마다 호출된다.
// 모드(step/next/continue)와 브레이크포인트에 따라 멈추고 명령을 처리한다.
class Debugger {
public:
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
