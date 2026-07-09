#pragma once

#include <ostream>
#include <string>

#include "../Assembler/AssemblerInterface.h"
#include "../Checker/CheckerInterface.h"
#include "../Executor/ExecuteInterface.h"
#include "../Tokenizer/TokenizeInterface.h"

// 공장 제어 쉘의 파일 모드(Architecture.md §9.2, Implement.md §5 "할 일 2").
// RunPromptShell처럼 한 줄씩 반복해서 읽는 대신, 파일 전체를 한 번에 읽어
// tokenize -> assemble -> check -> execute 파이프라인을 딱 한 번만 수행하고
// 종료한다. RunPromptShell과 동일하게 4개 *Interface에만 의존하므로, 4-Unit
// 구현체를 그대로 재사용하고 Mock으로 독립적으로 테스트할 수 있다.
//
// 한 번의 run() 호출은 항상 파일 1개(단일 파일)만 대상으로 한다 - 여러 경로를
// 나열해서 전달하는 기능은 없다. 여러 파일을 실행하려면 run()을 파일마다
// 따로 호출해야 한다.
class FileRunMode {
public:
    FileRunMode(TokenizeInterface& tokenizer, AssemblerInterface& assembler,
                CheckerInterface& checker, ExecuteInterface& executor);

    // filePath가 가리키는 파일 1개를 읽어 파이프라인을 한 번 실행한다.
    // - filePath가 존재하지 않거나 열 수 없으면 out에 오류 메시지를 출력하고
    //   false를 반환한다(파이프라인은 시작하지 않는다).
    // - filePath가 존재하지만 파일 1개(단일 파일)가 아니면(예: 디렉터리)
    //   out에 오류 메시지를 출력하고 false를 반환한다(파이프라인은 시작하지
    //   않는다).
    // - 파이프라인 도중 예외(AssemblyError/AssemblerError/CheckerError/
    //   ExecutorError 등 std::exception)가 발생하면 out에 메시지를 출력하고
    //   false를 반환한다. RunPromptShell과 달리 다음 줄을 계속 읽지 않고
    //   즉시 종료한다.
    // - 끝까지 성공하면 true를 반환한다.
    bool run(const std::string& filePath, std::ostream& out);

private:
    TokenizeInterface& tokenizer_;
    AssemblerInterface& assembler_;
    CheckerInterface& checker_;
    ExecuteInterface& executor_;
};
