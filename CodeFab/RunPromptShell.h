#pragma once

#include <istream>
#include <ostream>

#include "AssemblerInterface.h"
#include "CheckerInterface.h"
#include "ExecuteInterface.h"
#include "TokenizeInterface.h"

// CodeFab Prompt Shell.
// 4개의 Unit 인터페이스에 의존하여 한 줄씩 입력받아
// Tokenize -> Assemble -> Check -> Execute 파이프라인을 구동한다.
class RunPromptShell {
public:
    RunPromptShell(TokenizeInterface& tokenizer,
                   AssemblerInterface& assembler,
                   CheckerInterface& checker,
                   ExecuteInterface& executor);

    // in 에서 한 줄씩 읽어 실행하고 out 에 프롬프트/실행 결과/오류를 출력한다.
    // "exit" 입력 또는 EOF 시 종료한다.
    void Run(std::istream& in, std::ostream& out);

private:
    TokenizeInterface& tokenizer_;
    AssemblerInterface& assembler_;
    CheckerInterface& checker_;
    ExecuteInterface& executor_;
};
