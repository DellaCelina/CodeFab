#ifdef _DEBUG
#include "gmock/gmock.h"
#else

#include <iostream>

#include "../Tokenizer/Tokenizer.h"
#include "../Assembler/Assembler.h"
#include "../Assembler/FileSourceReader.h"
#include "../Checker/Checker.h"
#include "../Executor/Executor.h"
#include "CommandLineArgs.h"
#include "FileRunMode.h"
#include "RunPromptShell.h"

#endif


int main(int argc, char** argv) {
#ifdef _DEBUG
    (void)argc;
    (void)argv;
    testing::InitGoogleMock();
    return RUN_ALL_TESTS();
#else
    CommandLineArgs args;
    try {
        args = CommandLineArgs::parse(argc, argv);
    } catch (const std::invalid_argument& e) {
        std::cerr << e.what() << "\n\n" << CommandLineArgs::usageText();
        return 1;
    }

    if (args.mode == ShellMode::Help) {
        std::cout << CommandLineArgs::usageText();
        return 0;
    }

    Tokenizer tokenizer;
    FileSourceReader sourceReader(tokenizer);

    Assembler assembler(sourceReader);
    Executor executor;
    Checker checker(executor);

    switch (args.mode) {
        case ShellMode::Repl: {
            RunPromptShell shell(tokenizer, assembler, checker, executor);
            shell.run(std::cin, std::cout);
            return 0;
        }
        case ShellMode::Run: {
            FileRunMode mode(tokenizer, assembler, checker, executor);
            return mode.run(args.path, std::cout) ? 0 : 1;
        }
        case ShellMode::Debug:
            // 디버그 모드는 3일차 확장 3단계에서 구현 예정 (Architecture.md §9.3 참고).
            std::cerr << "이 모드는 아직 구현되지 않았습니다. --help로 사용 가능한 모드를 확인하세요.\n";
            return 1;
        default:
            return 1;
    }
#endif
}
