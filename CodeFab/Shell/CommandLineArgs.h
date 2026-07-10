#pragma once

#include <string>

enum class ShellMode { Repl, Run, Debug, Help };

struct CommandLineArgs {
    ShellMode mode = ShellMode::Repl;
    std::string path;  // Run/Debug 모드일 때만 사용된다.

    // argv[0]은 실행 파일 경로이므로 무시한다.
    //   (인자 없음)              -> Repl
    //   run <path>               -> Run,   path = <path>
    //   debug <path>             -> Debug, path = <path>
    //   --help | -h              -> Help  (path 없음, 다른 인자 무시)
    // run/debug인데 path가 없거나, 첫 인자가 run/debug/--help/-h 중 아무것도
    // 아니면 std::invalid_argument를 던진다.
    static CommandLineArgs parse(int argc, char** argv);

    // --help/-h 또는 파싱 오류 시 사용자에게 보여줄 모드 목록/사용법 안내문.
    static std::string usageText();
};
