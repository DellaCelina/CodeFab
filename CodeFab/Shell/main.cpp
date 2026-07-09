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
#include "DebugMode.h"
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
    Checker checker;

    switch (args.mode) {
        case ShellMode::Repl: {
            std::cout << "[mode: REPL] type 'exit' to quit.\n";
            RunPromptShell shell(tokenizer, assembler, checker, executor);
            shell.run(std::cin, std::cout);
            return 0;
        }
        case ShellMode::Run: {
            std::cout << "[mode: RUN]\n";
            FileRunMode mode(tokenizer, assembler, checker, executor);
            return mode.run(args.path, std::cout) ? 0 : 1;
        }
        case ShellMode::Debug: {
            std::cout << "[mode: DEBUG]\n";
            DebugMode mode(tokenizer, assembler, checker, executor);
            return mode.run(args.path, std::cin, std::cout) ? 0 : 1;
        }
        default:
            return 1;
    }
#endif
}
