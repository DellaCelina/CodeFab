#include "CommandLineArgs.h"

#include <stdexcept>

CommandLineArgs CommandLineArgs::parse(int argc, char** argv) {
    CommandLineArgs args;

    if (argc <= 1) {
        return args;  // 인자가 없으면 기존과 동일하게 Repl 모드로 시작한다.
    }

    const std::string modeArg = argv[1];

    if (modeArg == "run") {
        if (argc < 3) {
            throw std::invalid_argument(
                "run 모드는 실행할 파일 경로가 필요합니다. 사용법: CodeFab run <path>");
        }
        args.mode = ShellMode::Run;
        args.path = argv[2];
        return args;
    }

    if (modeArg == "debug") {
        if (argc < 3) {
            throw std::invalid_argument(
                "debug 모드는 실행할 파일 경로가 필요합니다. 사용법: CodeFab debug <path>");
        }
        args.mode = ShellMode::Debug;
        args.path = argv[2];
        return args;
    }

    throw std::invalid_argument(
        "알 수 없는 모드입니다: '" + modeArg
        + "' (run, debug 중 하나를 사용하세요. 인자가 없으면 REPL 모드로 시작합니다.)");
}
