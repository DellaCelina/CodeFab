#pragma once

#include <istream>
#include <ostream>
#include <string>

#include "../Assembler/AssemblerInterface.h"
#include "../Checker/CheckerInterface.h"
#include "../Checker/OptimizerInterface.h"
#include "../Executor/Executor.h"
#include "../Tokenizer/TokenizeInterface.h"

// FileRunMode와 동일한 파이프라인에 Debugger를 붙여 statement 단위로 멈추며
// step/next/continue/break/watch/inspect를 지원한다.
// setStatementHook이 Executor에만 있으므로 ExecuteInterface 대신 Executor&를 받는다.
class DebugMode {
public:
    DebugMode(TokenizeInterface& tokenizer, AssemblerInterface& assembler,
              CheckerInterface& checker, OptimizerInterface& optimizer, Executor& executor);

    // FileRunMode::run()과 동일한 규칙. in/out은 디버거 명령 전용이다.
    bool run(const std::string& filePath, std::istream& in, std::ostream& out);

private:
    TokenizeInterface& tokenizer_;
    AssemblerInterface& assembler_;
    CheckerInterface& checker_;
    OptimizerInterface& optimizer_;
    Executor& executor_;
};
