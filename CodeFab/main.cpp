#ifdef _DEBUG
#include "gmock/gmock.h"
#else

#include <iostream>

#include "AssemblerInterface.h"
#include "CheckerInterface.h"
#include "ExecuteInterface.h"
#include "ShellErrors.h"
#include "Tokenizer.h"

namespace {

// TODO: 각 담당자(Assembler/Checker/Executor)의 실제 구현으로 교체 예정.
// 지금은 Shell 통합 골격이 빌드/실행되는 것을 보여주기 위한 임시 구현이다.
class NotImplementedAssembler : public AssemblerInterface {
public:
    SyntaxTree Assemble(const std::vector<Token>&) override {
        throw AssemblyError(0, "Assembler가 아직 구현되지 않았습니다.");
    }
};

class NotImplementedChecker : public CheckerInterface {
public:
    bool Check(SyntaxTree&) override {
        throw CheckError(0, "Checker가 아직 구현되지 않았습니다.");
    }
};

class NotImplementedExecutor : public ExecuteInterface {
public:
    void Execute(SyntaxTree&) override {
        throw RuntimeCodeFabError(0, "Executor가 아직 구현되지 않았습니다.");
    }
};

}  // namespace

#endif

#include "RunPromptShell.h"

int main() {
#ifdef _DEBUG
    testing::InitGoogleMock();
    return RUN_ALL_TESTS();
#else
    Tokenizer tokenizer;
    NotImplementedAssembler assembler;
    NotImplementedChecker checker;
    NotImplementedExecutor executor;

    RunPromptShell shell(tokenizer, assembler, checker, executor);
    shell.Run(std::cin, std::cout);
    return 0;
#endif
}
