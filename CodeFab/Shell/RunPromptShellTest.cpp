#include "RunPromptShell.h"

#include <sstream>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "../Assembler/Assembler.h"
#include "../Assembler/FileSourceReader.h"
#include "../Checker/Checker.h"
#include "../Executor/Executor.h"
#include "../Tokenizer/Tokenizer.h"

// 이 파일은 RunPromptShell 자신의 로직(줄 버퍼링, '\' 줄 이음, 빈 줄/exit 처리,
// 프롬프트 출력 순서, 4-Unit 중 어디서 예외가 나든 한 줄 실패가 다음 줄 실행을
// 막지 않는지)을 검증하는 단위 테스트다. 예전에는 4개 *Interface를 gmock으로
// 대체해 "어떤 메서드가 몇 번 호출됐는지"를 확인했지만, 지금은 실제
// Tokenizer/Assembler(+FileSourceReader)/Checker/Executor 구현체를 그대로
// 사용해서 "실제로 어떤 결과가 나오는지"로 검증한다 - Mock으로만 재현 가능했던
// (실제 구현과 맞지 않는) 시나리오는 제거하거나 실제로 그 상황을 만들어내는
// 입력으로 바꿨다(각 테스트 주석 참고). 실제 파이프라인을 훨씬 더 깊게 검증하는
// 통합 테스트는 CodeFab/IntegrationTest/DebugIntegrationTest.cpp를 참고한다.
class RunPromptShellTest : public ::testing::Test {
protected:
    // 선언 순서 = 생성 순서: assembler가 참조로 물고 있는 sourceReader(와 그
    // 내부의 tokenizer), checker가 참조로 물고 있는 executor가 먼저 만들어져
    // 있어야 한다.
    Tokenizer tokenizer;
    FileSourceReader sourceReader{tokenizer};
    Assembler assembler{sourceReader};
    std::ostringstream programOutput;  // Executor가 print 결과를 쓰는 곳 (out과는 별개)
    Executor executor{programOutput};
    Checker checker{executor};

    RunPromptShell shell{tokenizer, assembler, checker, executor};

    void run(const std::string& input, std::ostringstream& out) {
        std::istringstream in(input);
        shell.run(in, out);
    }
};

TEST_F(RunPromptShellTest, EmptyInput_PrintsInitialPromptThenStops) {
    std::ostringstream out;

    run("", out);

    EXPECT_EQ(out.str(), ">>> ");
}

TEST_F(RunPromptShellTest, ExitCommand_TerminatesWithoutRunningRestOfInput) {
    // "exit"가 나오면 그 뒤에 이어지는 줄(여기서는 실제로 실행되면 출력이 남는
    // print문)은 전혀 처리되지 않아야 한다.
    std::ostringstream out;
    run("exit\nprint 1;\n", out);

    EXPECT_EQ(out.str(), ">>> ");
    EXPECT_EQ(programOutput.str(), "");
}

TEST_F(RunPromptShellTest, BlankLine_IsSkippedWithoutRunningPipeline) {
    // 공백만 있는 줄은 파이프라인을 타지 않고 그냥 다음 프롬프트로 넘어가야
    // 한다. 만약 실수로 파이프라인을 태운다면 토큰이 없는 채로 Assembler가
    // "Expect expression." 같은 오류를 던져 아래 기대값과 달라진다.
    std::ostringstream out;
    run("   \n", out);

    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellTest, SingleCompleteLine_ExecutesSuccessfullyAndPrintsNextPrompt) {
    std::ostringstream out;
    run("var a = 3;\n", out);

    EXPECT_EQ(out.str(), ">>> >>> ");
    EXPECT_EQ(programOutput.str(), "");
}

TEST_F(RunPromptShellTest, MultipleLines_ShareStateAcrossLinesUsingSameExecutorInstance) {
    // 같은 Executor 인스턴스가 세션 전체에 걸쳐 재사용된다는 것을, 실제로 한
    // 줄에서 선언한 변수를 다음 줄에서 읽어보는 것으로 직접 확인한다(Mock으로는
    // "몇 번 호출됐는지"만 셀 수 있었지만, 실제 구현으로는 상태가 정말
    // 유지되는지까지 검증할 수 있다).
    std::ostringstream out;
    run("var a = 3;\nprint a;\n", out);

    EXPECT_EQ(out.str(), ">>> >>> >>> ");
    EXPECT_EQ(programOutput.str(), "3\n");
}

TEST_F(RunPromptShellTest, UnterminatedString_WaitsForMoreInputWhenTokenizerReportsIncomplete) {
    // 실제 Tokenizer가 IncompleteInputError를 던지는 유일한 경우는 문자열
    // 리터럴이 닫는 따옴표 없이 끝나는 경우다(괄호/중괄호 깊이는 더 이상
    // 추적하지 않는다). 이 신호를 받으면 셸은 버퍼를 비우지 않고 다음 줄을
    // 이어받는다.
    std::ostringstream out;
    run("print \"hello\n", out);

    EXPECT_EQ(out.str(), ">>> ... ");
    EXPECT_EQ(programOutput.str(), "");
}

TEST_F(RunPromptShellTest, MultilineString_CompletesWhenClosingQuoteArrives) {
    std::ostringstream out;
    run("print \"hello\nworld\";\n", out);

    EXPECT_EQ(out.str(), ">>> ... >>> ");
    EXPECT_EQ(programOutput.str(), "hello\nworld\n");
}

TEST_F(RunPromptShellTest, LineEndingWithBackslash_WaitsForNextLineWithoutRunningPipeline) {
    std::ostringstream out;
    run("var a = \\\n", out);

    EXPECT_EQ(out.str(), ">>> ... ");
    EXPECT_EQ(programOutput.str(), "");
}

TEST_F(RunPromptShellTest, MultilineViaBackslash_JoinsLinesAndRunsOnceOnFinalLine) {
    std::ostringstream out;
    run("var a =\\\n3;\n", out);

    EXPECT_EQ(out.str(), ">>> ... >>> ");
    EXPECT_EQ(programOutput.str(), "");
}

TEST_F(RunPromptShellTest, MultilineViaBackslash_AcrossThreeLinesJoinsAllBeforeRunning) {
    std::ostringstream out;
    run("var a =\\\n1 +\\\n2;\n", out);

    EXPECT_EQ(out.str(), ">>> ... ... >>> ");
    EXPECT_EQ(programOutput.str(), "");
}

TEST_F(RunPromptShellTest, TokenizeError_IsReportedAndRestOfPipelineIsSkipped) {
    // 실제 Tokenizer는 인식 불가능한 문자를 만나면 줄 번호를 메시지에 직접 담아
    // AssemblyError를 던진다 (Tokenizer.cpp의 scanDefault() 참고).
    std::ostringstream out;
    run("var a = @;\n", out);

    EXPECT_EQ(out.str(), ">>> [1번째 줄] 알 수 없는 문자: '@'\n>>> ");
    EXPECT_EQ(programOutput.str(), "");
}

TEST_F(RunPromptShellTest, AssemblerErrorThrown_IsReportedAndCheckerExecutorAreSkipped) {
    // 실제 Assembler는 '+' 다음에 피연산자가 없으면 AssemblerError를 던진다.
    std::ostringstream out;
    run("var a = 3 + ;\n", out);

    EXPECT_EQ(out.str(), ">>> Expect expression. (near ';' at line 1)\n>>> ");
    EXPECT_EQ(programOutput.str(), "");
}

TEST_F(RunPromptShellTest, CheckerErrorThrown_IsReportedAndExecutorIsSkipped) {
    // Checker의 전역 스코프는 세션 전체에 걸쳐 유지되므로, 같은 이름을 다음
    // 줄에서 또 선언하면 실제로 "중복 선언" CheckerError가 발생한다.
    std::ostringstream out;
    run("var a = 12;\nvar a = 13;\n", out);

    EXPECT_EQ(out.str(),
              ">>> >>> [1번째 줄] 'a'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.\n>>> ");
    EXPECT_EQ(programOutput.str(), "");
}

TEST_F(RunPromptShellTest, RuntimeError_IsReportedAndShellKeepsRunning) {
    // 실제 Executor는 0으로 나누면 ExecutorError를 던진다. 이 오류가 나도
    // 셸은 죽지 않고 다음 줄을 계속 처리해야 한다.
    std::ostringstream out;
    run("var a = 3 / 0;\nprint 1 + 2;\n", out);

    EXPECT_EQ(out.str(), ">>> 0으로 나눌 수 없습니다\n>>> >>> ");
    EXPECT_EQ(programOutput.str(), "3\n");
}

TEST_F(RunPromptShellTest, ErrorOnOneLine_DoesNotPreventNextLineFromRunning) {
    // 선언되지 않은 변수에 대입하면 Checker가 "선언되지 않은 변수" 오류를
    // 던진다. 이 오류가 나도 다음 줄(정상적인 변수 선언)은 그대로 처리된다.
    std::ostringstream out;
    run("x = 5;\nvar y = 1;\n", out);

    EXPECT_EQ(out.str(), ">>> [1번째 줄] 'x'에러: 선언되지 않은 변수입니다.\n>>> >>> ");
    EXPECT_EQ(programOutput.str(), "");
}
