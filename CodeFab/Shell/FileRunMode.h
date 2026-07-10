#pragma once

#include <ostream>
#include <string>

#include "../Assembler/AssemblerInterface.h"
#include "../Checker/CheckerInterface.h"
#include "../Checker/OptimizerInterface.h"
#include "../Executor/ExecuteInterface.h"
#include "../Tokenizer/TokenizeInterface.h"

// 파일 전체를 한 번에 읽어 tokenize → assemble → check → execute 파이프라인을
// 한 번 실행한다. run() 한 번에 파일 1개만 처리한다.
class FileRunMode {
public:
    FileRunMode(TokenizeInterface& tokenizer, AssemblerInterface& assembler,
                CheckerInterface& checker, OptimizerInterface& optimizer,
                ExecuteInterface& executor);

    // 성공하면 true, 파일을 열 수 없거나 파이프라인 도중 오류가 나면 out에
    // 오류 메시지를 출력하고 false를 반환한다.
    bool run(const std::string& filePath, std::ostream& out);

private:
    TokenizeInterface& tokenizer_;
    AssemblerInterface& assembler_;
    CheckerInterface& checker_;
    OptimizerInterface& optimizer_;
    ExecuteInterface& executor_;
};
