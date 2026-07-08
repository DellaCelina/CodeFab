#ifdef _DEBUG
#include "gmock/gmock.h"
#else

#include <iostream>

#include "../Tokenizer/Tokenizer.h"
#include "../Assembler/Assembler.h"
#include "../Assembler/FileSourceReader.h"
#include "../Checker/Checker.h"
#include "../Executor/Executor.h"
#include "RunPromptShell.h"

#endif


int main() {
#ifdef _DEBUG
    testing::InitGoogleMock();
    return RUN_ALL_TESTS();
#else
    Tokenizer tokenizer;
    FileSourceReader sourceReader(tokenizer);

    Assembler assembler(sourceReader);
    Executor executor;
    Checker checker(executor);

    RunPromptShell shell(tokenizer, assembler, checker, executor);
    shell.run(std::cin, std::cout);
    return 0;
#endif
}
