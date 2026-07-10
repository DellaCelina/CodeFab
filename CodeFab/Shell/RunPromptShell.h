#pragma once

#include <istream>
#include <ostream>

#include "../Assembler/AssemblerInterface.h"
#include "../Checker/CheckerInterface.h"
#include "../Checker/OptimizerInterface.h"
#include "../Executor/ExecuteInterface.h"
#include "../Tokenizer/TokenizeInterface.h"

// 한 줄씩 입력받아 tokenize → assemble → check → optimize → execute 파이프라인을 구동한다.
class RunPromptShell {
public:
    RunPromptShell(TokenizeInterface& tokenizer,
                   AssemblerInterface& assembler,
                   CheckerInterface& checker,
                   OptimizerInterface& optimizer,
                   ExecuteInterface& executor);

    // in 에서 한 줄씩 읽어 실행하고 out 에 프롬프트/실행 결과/오류를 출력한다.
    // "exit" 입력 또는 EOF 시 종료한다.
    void run(std::istream& in, std::ostream& out);

private:
    TokenizeInterface& tokenizer_;
    AssemblerInterface& assembler_;
    CheckerInterface& checker_;
    OptimizerInterface& optimizer_;
    ExecuteInterface& executor_;
};
