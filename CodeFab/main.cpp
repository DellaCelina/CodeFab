#ifdef _DEBUG
#include "gmock/gmock.h"
#else

#include <iostream>

#include "Tokenizer.h"
#include "Assembler.h"
#include "Checker.h"
#include "Executor.h"
#include "ShellErrors.h"
#include "RunPromptShell.h"

#endif

int main() {
#ifdef _DEBUG
    testing::InitGoogleMock();
    return RUN_ALL_TESTS();
#else
    Tokenizer tokenizer;
    Assembler assembler;
    Checker checker;
    Executor executor;

    RunPromptShell shell(tokenizer, assembler, checker, executor);
    shell.run(std::cin, std::cout);
    return 0;
#endif
}
