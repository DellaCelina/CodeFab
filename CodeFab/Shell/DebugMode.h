#pragma once

#include <istream>
#include <ostream>
#include <string>

#include "../Assembler/AssemblerInterface.h"
#include "../Checker/CheckerInterface.h"
#include "../Checker/OptimizerInterface.h"
#include "../Executor/Executor.h"
#include "../Tokenizer/TokenizeInterface.h"

// 공장 제어 쉘의 디버그 모드(Architecture.md §9.3, Implement.md §5 "할 일 3").
// FileRunMode와 동일하게 파일 전체를 한 번에 읽어 tokenize -> assemble ->
// check -> execute 파이프라인을 한 번만 수행하지만, 실행 직전에 Debugger를
// Executor::setStatementHook에 붙여서 statement 단위로 멈춰가며
// step/next/continue/breakpoint/watch/inspect를 지원한다.
//
// FileRunMode(ExecuteInterface&를 받음)와 달리 구체 타입 Executor&를 받는다 -
// setStatementHook()이 ExecuteInterface에는 없고 Executor에만 있기 때문이다
// (Executor.h 참고, Implement.md §5도 이 차이를 이미 명시해뒀다).
class DebugMode {
public:
    DebugMode(TokenizeInterface& tokenizer, AssemblerInterface& assembler,
              CheckerInterface& checker, OptimizerInterface& optimizer, Executor& executor);

    // filePath가 가리키는 파일 1개를 읽어, Debugger를 붙인 채로 파이프라인을
    // 한 번 실행한다. in/out은 디버거의 명령 입력/출력 전용이다(프로그램 자체의
    // print 결과는 Executor 생성자에 전달된 스트림에 그대로 쓰인다 -
    // FileRunMode와 동일).
    // 파일을 열 수 없거나 파일 1개(단일 파일)가 아니거나 파이프라인 도중
    // 예외/실패가 나면 out에 오류를 출력하고 false를 반환한다. 끝까지 성공하면
    // true를 반환한다 (FileRunMode::run()과 동일한 규칙).
    bool run(const std::string& filePath, std::istream& in, std::ostream& out);

private:
    TokenizeInterface& tokenizer_;
    AssemblerInterface& assembler_;
    CheckerInterface& checker_;
    OptimizerInterface& optimizer_;
    Executor& executor_;
};
