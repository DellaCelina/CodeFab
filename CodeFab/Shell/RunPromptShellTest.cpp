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

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::StartsWith;

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
    // 내부의 tokenizer)가 먼저 만들어져 있어야 한다.
    Tokenizer tokenizer;
    FileSourceReader sourceReader{tokenizer};
    Assembler assembler{sourceReader};
    std::ostringstream programOutput;  // Executor가 print 결과를 쓰는 곳 (out과는 별개)
    Executor executor{programOutput};
    Checker checker;

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

TEST_F(RunPromptShellTest, UnterminatedString_ReportsError) {
    std::ostringstream out;
    run("print \"hello\n", out);

    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "), HasSubstr("unterminated string literal.")));
    EXPECT_EQ(programOutput.str(), "");
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

    EXPECT_EQ(out.str(), ">>> [line 1] unknown character: '@'\n>>> ");
    EXPECT_EQ(programOutput.str(), "");
}

TEST_F(RunPromptShellTest, AssemblerErrorThrown_IsReportedAndCheckerExecutorAreSkipped) {
    // 실제 Assembler는 '+' 다음에 피연산자가 없으면 AssemblerError를 던진다.
    std::ostringstream out;
    run("var a = 3 + ;\n", out);

    EXPECT_EQ(out.str(), ">>> [line 1] Expect expression. (near ';')\n>>> ");
    EXPECT_EQ(programOutput.str(), "");
}

TEST_F(RunPromptShellTest, CheckerErrorThrown_IsReportedAndExecutorIsSkipped) {
    // Checker의 전역 스코프는 세션 전체에 걸쳐 유지되므로, 같은 이름을 다음
    // 줄에서 또 선언하면 실제로 "중복 선언" CheckerError가 발생한다.
    std::ostringstream out;
    run("var a = 12;\nvar a = 13;\n", out);

    EXPECT_EQ(out.str(),
              ">>> >>> [line 1] 'a' is already declared in this scope.\n>>> ");
    EXPECT_EQ(programOutput.str(), "");
}

TEST_F(RunPromptShellTest, RuntimeError_IsReportedAndShellKeepsRunning) {
    // 실제 Executor는 0으로 나누면 ExecutorError를 던진다. 이 오류가 나도
    // 셸은 죽지 않고 다음 줄을 계속 처리해야 한다.
    std::ostringstream out;
    run("var a = 3 / 0;\nprint 1 + 2;\n", out);

    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "), HasSubstr("division by zero.")));
    EXPECT_EQ(programOutput.str(), "3\n");
}

TEST_F(RunPromptShellTest, ErrorOnOneLine_DoesNotPreventNextLineFromRunning) {
    // 선언되지 않은 변수에 대입하면 Checker가 "선언되지 않은 변수" 오류를
    // 던진다. 이 오류가 나도 다음 줄(정상적인 변수 선언)은 그대로 처리된다.
    std::ostringstream out;
    run("x = 5;\nvar y = 1;\n", out);

    EXPECT_EQ(out.str(), ">>> [line 1] 'x' is not declared.\n>>> >>> ");
    EXPECT_EQ(programOutput.str(), "");
}

// 아래부터는 실제 버그 리포트("REPL에서 여러 줄에 걸친 if/블록을 입력하면 블록
// 뒤의 문장이 실행되지 않는다")를 재현/검증하는 테스트다. 원인은 예전에는 각
// getline() 한 줄을 곧바로 "완결된 문장"으로 간주해 파이프라인을 태웠기 때문에,
// if/블록처럼 여러 줄에 걸친 문장이 줄마다 서로 무관한 파싱 단위로 쪼개졌던
// 것이다. 이제는 AssemblerError가 "아직 토큰이 부족해서(EOF)" 발생했는지
// ("[line " 접두어가 없는지)를 보고 문장이 아직 안 끝났으면 계속 다음 줄을
// 이어받는다(RunPromptShell.cpp의 IsIncompleteInputError() 참고).

TEST_F(RunPromptShellTest, MultilineIfBlock_WithoutBackslash_RunsAsOneStatementAndKeepsGlobalAAfterBlock) {
    // 버그 리포트에 나온 시나리오 그대로: if 블록 안에서 a를 "hi"로 shadow 해도
    // 블록을 빠져나온 뒤의 print a;는 바깥(전역) 스코프의 5를 그대로 봐야 한다.
    std::ostringstream out;
    run(
        "var a=5;\n"
        "if (true)\n"
        "{\n"
        "   print a;\n"
        "   var a=\"hi\";\n"
        "   print a;\n"
        "}\n"
        "print a;\n",
        out);

    // if/블록이 완결되기 전까지는 매줄 "... " 연속 프롬프트만 찍히고, 블록이
    // 닫히는 '}'를 만난 시점에 실행되어 ">>> "로 돌아온다. 그 뒤 print a;는
    // 새로운 완결 문장이므로 곧바로 실행된다.
    EXPECT_EQ(out.str(), ">>> >>> ... ... ... ... ... >>> >>> ");
    EXPECT_EQ(programOutput.str(), "5\nhi\n5\n");
}

TEST_F(RunPromptShellTest, MultilineIfWithoutBraces_WaitsForBodyOnNextLine) {
    // 블록 없이 단일 문장을 본문으로 갖는 if도 (조건까지만 입력된 상태로는
    // 아직 본문이 없으므로) 다음 줄을 계속 기다려야 한다.
    std::ostringstream out;
    run("if (true)\nprint 1;\n", out);

    EXPECT_EQ(out.str(), ">>> ... >>> ");
    EXPECT_EQ(programOutput.str(), "1\n");
}

TEST_F(RunPromptShellTest, MultilineForLoop_WaitsUntilClosingBraceThenRunsOnce) {
    std::ostringstream out;
    run(
        "for (var i = 0; i < 3; i = i + 1)\n"
        "{\n"
        "   print i;\n"
        "}\n",
        out);

    EXPECT_EQ(out.str(), ">>> ... ... ... >>> ");
    EXPECT_EQ(programOutput.str(), "0\n1\n2\n");
}

TEST_F(RunPromptShellTest, NestedMultilineBlocks_WaitUntilOutermostBlockCloses) {
    std::ostringstream out;
    run(
        "if (true)\n"
        "{\n"
        "   if (true)\n"
        "   {\n"
        "      print 1;\n"
        "   }\n"
        "   print 2;\n"
        "}\n",
        out);

    EXPECT_EQ(out.str(), ">>> ... ... ... ... ... ... ... >>> ");
    EXPECT_EQ(programOutput.str(), "1\n2\n");
}

TEST_F(RunPromptShellTest, UnmatchedClosingBrace_ReportsErrorImmediatelyInsteadOfWaitingForever) {
    // 실제 문법 오류(토큰은 있는데 종류가 틀림)는 "[line N] ..." 형식이므로
    // EOF로 오인해 무한히 다음 줄을 기다리면 안 되고, 즉시 오류를 보고해야 한다.
    std::ostringstream out;
    run("}\n", out);

    EXPECT_EQ(out.str(), ">>> [line 1] Expect expression. (near '}')\n>>> ");
    EXPECT_EQ(programOutput.str(), "");
}

TEST_F(RunPromptShellTest, MultilineBlock_ErrorAfterClosingBraceDoesNotPreventNextLineFromRunning) {
    // 블록이 완결되면서 실행 중 오류(0으로 나누기)가 나도, 그 오류가 셀을
    // 죽이거나 다음 줄 처리를 막지 않아야 한다.
    std::ostringstream out;
    run(
        "if (true)\n"
        "{\n"
        "   print 1 / 0;\n"
        "}\n"
        "print 42;\n",
        out);

    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "), HasSubstr("division by zero.")));
    EXPECT_EQ(programOutput.str(), "42\n");
}
