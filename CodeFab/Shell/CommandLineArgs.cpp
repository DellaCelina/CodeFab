#include "CommandLineArgs.h"

#include <stdexcept>

CommandLineArgs CommandLineArgs::parse(int argc, char** argv) {
    CommandLineArgs args;

    if (argc <= 1) {
        return args;  // 인자가 없으면 기존과 동일하게 Repl 모드로 시작한다.
    }

    const std::string modeArg = argv[1];

    if (modeArg == "--help" || modeArg == "-h") {
        args.mode = ShellMode::Help;
        return args;
    }

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
        + "' (run, debug 중 하나를 사용하세요. 인자가 없으면 REPL 모드로 시작합니다. "
          "사용법을 보려면 --help를 사용하세요.)");
}

std::string CommandLineArgs::usageText() {
    return
        "CodeFab - 공장 제어 쉘\n"
        "\n"
        "사용법:\n"
        "  CodeFab                  프롬프트(REPL) 모드로 시작합니다.\n"
        "                           한 줄씩 입력받아 즉시 실행 결과를 보여줍니다.\n"
        "  CodeFab run <path>       파일 모드. <path>의 소스 파일 전체를 한 번에\n"
        "                           실행하고 종료합니다.\n"
        "  CodeFab debug <path>     디버그 모드. <path>의 소스 파일을 문장 단위로\n"
        "                           멈춰가며(breakpoint/step/watch) 실행합니다.\n"
        "  CodeFab --help, -h       이 도움말을 출력합니다.\n";
}
