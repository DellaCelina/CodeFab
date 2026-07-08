#pragma once

#include <string>

// 공장 제어 쉘의 실행 모드. 3일차 확장(Architecture.md §9.1)의 1단계 "모드 분리"에서
// 도입한다. Run/Debug 모드의 실제 동작(FileRunMode/DebugMode)은 이후 단계에서 구현하고,
// 이 파일은 그 전 단계로 "argv를 어떤 모드로 해석할지"만 담당한다.
enum class ShellMode { Repl, Run, Debug };

struct CommandLineArgs {
    ShellMode mode = ShellMode::Repl;
    std::string path;  // Run/Debug 모드일 때만 사용된다.

    // argv[0]은 실행 파일 경로이므로 무시한다.
    //   (인자 없음)              -> Repl
    //   run <path>               -> Run,   path = <path>
    //   debug <path>             -> Debug, path = <path>
    // run/debug인데 path가 없거나, 첫 인자가 run/debug가 아니면
    // std::invalid_argument를 던진다.
    static CommandLineArgs parse(int argc, char** argv);
};
