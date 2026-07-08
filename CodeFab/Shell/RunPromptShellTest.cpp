#include "RunPromptShell.h"

#include <sstream>
#include <string>
#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "../Assembler/Assembler.h"
#include "../Assembler/AssemblerInterface.h"
#include "../Assembler/FileSourceReader.h"
#include "../Checker/Checker.h"
#include "../Checker/CheckerInterface.h"
#include "../Executor/ExecuteInterface.h"
#include "../Executor/Executor.h"
#include "../Assembler/SyntaxTree.h"
#include "../Tokenizer/Token.h"
#include "../Tokenizer/TokenizeInterface.h"
#include "../Tokenizer/Tokenizer.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StartsWith;
using ::testing::Throw;

namespace {

class MockTokenizer : public TokenizeInterface {
public:
    MOCK_METHOD(std::vector<Token>, tokenize, (const std::string& source), (override));
};

class MockAssembler : public AssemblerInterface {
public:
    MOCK_METHOD(SyntaxTree, assemble, (const std::vector<Token>& tokens), (override));
};

class MockChecker : public CheckerInterface {
public:
    MOCK_METHOD(bool, check, (SyntaxTree & tree), (override));
};

class MockExecutor : public ExecuteInterface {
public:
    MOCK_METHOD(void, execute, (SyntaxTree & tree), (override));
    MOCK_METHOD(Value, evaluate, (Expression * expr), (override));
    MOCK_METHOD(const Environment&, environment, (), (const, override));
};

class RunPromptShellTest : public ::testing::Test {
protected:
    NiceMock<MockTokenizer> tokenizer;
    NiceMock<MockAssembler> assembler;
    NiceMock<MockChecker> checker;
    NiceMock<MockExecutor> executor;

    RunPromptShell shell{tokenizer, assembler, checker, executor};

    void run(const std::string& input, std::ostringstream& out) {
        std::istringstream in(input);
        shell.run(in, out);
    }
};

}  // namespace

TEST_F(RunPromptShellTest, EmptyInput_PrintsInitialPromptThenStops) {
    std::ostringstream out;

    run("", out);

    EXPECT_EQ(out.str(), ">>> ");
}

TEST_F(RunPromptShellTest, ExitCommand_TerminatesWithoutTokenizing) {
    EXPECT_CALL(tokenizer, tokenize(_)).Times(0);

    std::ostringstream out;
    run("exit\nvar a = 3;\n", out);

    EXPECT_EQ(out.str(), ">>> ");
}

TEST_F(RunPromptShellTest, BlankLine_IsSkippedWithoutTokenizing) {
    EXPECT_CALL(tokenizer, tokenize(_)).Times(0);

    std::ostringstream out;
    run("   \n", out);

    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellTest, SingleCompleteLine_CallsPipelineInOrder) {
    InSequence seq;
    EXPECT_CALL(tokenizer, tokenize(std::string("var a = 3;")))
        .WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    run("var a = 3;\n", out);

    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellTest, SameTreeInstance_FlowsFromAssembleThroughCheckAndExecute) {
    SyntaxTree* capturedInCheck = nullptr;

    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_)).WillOnce(Invoke([&capturedInCheck](SyntaxTree& tree) {
        capturedInCheck = &tree;
        return true;
    }));
    EXPECT_CALL(executor, execute(_)).WillOnce(Invoke([&capturedInCheck](SyntaxTree& tree) {
        EXPECT_EQ(&tree, capturedInCheck);
    }));

    std::ostringstream out;
    run("var a = 3;\n", out);
}

TEST_F(RunPromptShellTest, MultipleLines_ProcessedOneAtATimeUsingSameExecutorInstance) {
    EXPECT_CALL(tokenizer, tokenize(std::string("var a = 3;")))
        .WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(tokenizer, tokenize(std::string("print a;")))
        .WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_))
        .WillOnce(Return(ByMove(SyntaxTree())))
        .WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_)).Times(2).WillRepeatedly(Return(true));

    // 같은 executor 인스턴스가 두 번 호출된다 -> 변수 저장소(Environment)가
    // Executor 내부에서 세션 전체에 걸쳐 유지되는 구조를 보장한다.
    EXPECT_CALL(executor, execute(_)).Times(2);

    std::ostringstream out;
    run("var a = 3;\nprint a;\n", out);

    EXPECT_EQ(out.str(), ">>> >>> >>> ");
}

TEST_F(RunPromptShellTest, UnbalancedBrace_WaitsForMoreInputWhenTokenizerReportsIncomplete) {
    EXPECT_CALL(tokenizer, tokenize(std::string("if (a > 0) {")))
        .WillOnce(Throw(IncompleteInputError("입력이 완결되지 않았습니다.")));

    std::ostringstream out;
    run("if (a > 0) {\n", out);

    EXPECT_EQ(out.str(), ">>> ... ");
}

TEST_F(RunPromptShellTest, MultilineBlock_CompletesWhenBraceClosesAndTokenizerStopsReportingIncomplete) {
    InSequence seq;
    EXPECT_CALL(tokenizer, tokenize(std::string("if (a > 0) {")))
        .WillOnce(Throw(IncompleteInputError("입력이 완결되지 않았습니다.")));
    EXPECT_CALL(tokenizer, tokenize(std::string("if (a > 0) {\nprint a;")))
        .WillOnce(Throw(IncompleteInputError("입력이 완결되지 않았습니다.")));
    EXPECT_CALL(tokenizer, tokenize(std::string("if (a > 0) {\nprint a;\n}")))
        .WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    run("if (a > 0) {\nprint a;\n}\n", out);

    EXPECT_EQ(out.str(), ">>> ... ... >>> ");
}

TEST_F(RunPromptShellTest, LineEndingWithBackslash_WaitsForNextLineWithoutTokenizing) {
    EXPECT_CALL(tokenizer, tokenize(_)).Times(0);

    std::ostringstream out;
    run("var a = \\\n", out);

    EXPECT_EQ(out.str(), ">>> ... ");
}

TEST_F(RunPromptShellTest, MultilineViaBackslash_JoinsLinesAndTokenizesOnceOnFinalLine) {
    InSequence seq;
    EXPECT_CALL(tokenizer, tokenize(std::string("var a =\n3;")))
        .WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    run("var a =\\\n3;\n", out);

    EXPECT_EQ(out.str(), ">>> ... >>> ");
}

TEST_F(RunPromptShellTest, MultilineViaBackslash_AcrossThreeLinesJoinsAllBeforeTokenizing) {
    EXPECT_CALL(tokenizer, tokenize(std::string("var a =\n1 +\n2;")))
        .WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    run("var a =\\\n1 +\\\n2;\n", out);

    EXPECT_EQ(out.str(), ">>> ... ... >>> ");
}

TEST_F(RunPromptShellTest, TokenizeError_IsReportedAndRestOfPipelineIsSkipped) {
    // 실제 Tokenizer는 인식 불가능한 문자를 만나면 줄 번호를 메시지에 직접 담아
    // AssemblyError를 던진다 (TokenizeInterface.h/Tokenizer.cpp 참고).
    EXPECT_CALL(tokenizer, tokenize(_))
        .WillOnce(Throw(AssemblyError("[1번째 줄] 인식할 수 없는 문자입니다.")));
    EXPECT_CALL(assembler, assemble(_)).Times(0);
    EXPECT_CALL(checker, check(_)).Times(0);
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    run("var a = @;\n", out);

    EXPECT_EQ(out.str(), ">>> [1번째 줄] 인식할 수 없는 문자입니다.\n>>> ");
}

TEST_F(RunPromptShellTest, AssemblerErrorThrown_IsReportedAndCheckerExecutorAreSkipped) {
    // 실제 Assembler는 문법 오류를 만나면 AssemblerError를 던진다
    // (AssemblerInterface.h 참고). AssemblerError는 줄 번호를 따로 들고 있지 않는
    // 순수 메시지 예외라서, 필요하면 makeParseError()처럼 메시지에 위치 정보를
    // 직접 담는다 - 이 테스트에서는 위치 정보 없는 메시지를 그대로 재현한다.
    EXPECT_CALL(tokenizer, tokenize(_)).WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_))
        .WillOnce(Throw(AssemblerError("'+' 다음에 피연산자가 필요합니다.")));
    EXPECT_CALL(checker, check(_)).Times(0);
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    run("var a = 3 + ;\n", out);

    EXPECT_EQ(out.str(), ">>> '+' 다음에 피연산자가 필요합니다.\n>>> ");
}

TEST_F(RunPromptShellTest, CheckerErrorThrown_IsReportedAndExecutorIsSkipped) {
    // 실제 Checker는 의미 오류를 만나면 줄 번호를 메시지에 직접 담아 CheckerError를
    // 던진다 (checker.cpp의 reportError() 참고).
    EXPECT_CALL(tokenizer, tokenize(_)).WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_))
        .WillOnce(Throw(CheckerError("[2번째 줄] 'a'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.")));
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    run("var a = 12;\n", out);

    EXPECT_EQ(out.str(),
              ">>> [2번째 줄] 'a'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.\n>>> ");
}

TEST_F(RunPromptShellTest, CheckReturnsFalseWithoutThrowing_SkipsExecutorAndReportsFailure) {
    EXPECT_CALL(tokenizer, tokenize(_)).WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_)).WillOnce(Return(false));
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    run("var a = 12;\n", out);

    EXPECT_EQ(out.str(), ">>> 코드 검사에 실패했습니다.\n>>> ");
}

TEST_F(RunPromptShellTest, RuntimeError_IsReportedAndShellKeepsRunning) {
    EXPECT_CALL(tokenizer, tokenize(_)).Times(2).WillRepeatedly(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_))
        .WillOnce(Return(ByMove(SyntaxTree())))
        .WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_)).Times(2).WillRepeatedly(Return(true));
    // 실제 Executor 구현체가 던지는 예외는 ExecutorError다 (ExecuteInterface.h 참고).
    // ExecutorError는 line() 정보가 없는 순수 std::exception이라, Shell은 이를
    // "[N번째 줄]" 접두사 없이 메시지만 출력한다.
    EXPECT_CALL(executor, execute(_))
        .WillOnce(Throw(ExecutorError("0으로 나눈 오류")))
        .WillOnce(Return());

    std::ostringstream out;
    run("a = 3 / 0;\nprint a;\n", out);

    EXPECT_EQ(out.str(), ">>> 0으로 나눈 오류\n>>> >>> ");
}

TEST_F(RunPromptShellTest, ErrorOnOneLine_DoesNotPreventNextLineFromRunning) {
    // 실제 Tokenizer 구현체는 인식 불가능한 문자를 만나면 줄 번호를 메시지에 직접
    // 담아 AssemblyError를 던진다 (TokenizeInterface.h/Tokenizer.cpp 참고).
    EXPECT_CALL(tokenizer, tokenize(std::string("x = 5;")))
        .WillOnce(Throw(AssemblyError("[1번째 줄] 미정의된 변수 'x'")));
    EXPECT_CALL(tokenizer, tokenize(std::string("var y = 1;")))
        .WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    run("x = 5;\nvar y = 1;\n", out);

    EXPECT_EQ(out.str(), ">>> [1번째 줄] 미정의된 변수 'x'\n>>> >>> ");
}

// ============================================================================
// Integration Test (시나리오 출처: https://gist.github.com/aijeonghwan-star/d1535e870aeb6a4a928142d4d57c191e)
//
// Tokenizer/Assembler/Checker/Executor 전부 실제 구현을 사용한다 (더 이상 Mock 없음).
// Executor는 생성자로 받은 스트림(programOutput)에 프로그램의 print 결과를 쓰고,
// 셸의 프롬프트/에러 스트림(out)과는 분리되어 있다 -> 두 스트림을 각각 검증한다.
//
// 현재 구현의 실제 한계 (아래 테스트 중 일부는 이 한계 때문에 스펙(gist)이
// 기대하는 값과 다르게 실패한다 - 각 테스트 옆 주석 참고):
// - Tokenizer/Assembler/Checker/Executor가 던지는 예외(AssemblyError, AssemblerError,
//   CheckerError, ExecutorError)는 전부 각자의 인터페이스 헤더(TokenizeInterface.h,
//   AssemblerInterface.h, CheckerInterface.h, ExecuteInterface.h)에 정의된, 줄 번호를
//   따로 들고 있지 않는 순수 std::exception이다. 줄 번호가 필요하면(Checker, 그리고
//   Tokenizer의 AssemblyError) 호출부가 메시지에 직접 담는다. Shell은 이들을
//   catch(const std::exception&) 하나로 잡아 메시지만 그대로 출력한다
//   (RunPromptShell.cpp 참고) - gist가 요구하는 영어 메시지가 아니라 각 Unit이
//   실제로 던지는 한글 메시지가 그대로 노출된다.
// - Checker는 BlockStatement/DeclareStatement/PrintStatement (그리고 그 안의
//   IdentifierExpression/BinaryExpression)만 검사한다. If/For/Assign 등은 아직
//   검사하지 않고 통과시킨다.
// - Executor는 PrintStatement, DeclareStatement, BlockStatement, IfStatement,
//   ForStatement, 리터럴/산술/비교/대입 연산자를 모두 처리한다. 타입이 맞지 않는
//   산술 연산은 ExecutorError(다듬어진 영어 메시지가 아니라 한글 메시지, 예:
//   "타입 오류: ...")를 던진다.
// ============================================================================
class RunPromptShellIntegrationTest : public ::testing::Test {
protected:
    // 선언 순서 = 생성 순서: assembler/checker가 참조로 물고 있는 sourceReader
    // (와 그 내부의 tokenizer), executor가 먼저 만들어져 있어야 한다.
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

// --- 1. 정상 동작 시나리오: 실제 파이프라인을 끝까지 돌려 실제 출력값을 검증 ---

TEST_F(RunPromptShellIntegrationTest, ArithmeticPrecedence_PrintsSeven) {
    std::ostringstream out;
    run("print 1 + 2 * 3;\n", out);

    EXPECT_EQ(programOutput.str(), "7\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, ParenthesizedExpression_PrintsNine) {
    std::ostringstream out;
    run("print (1 + 2) * 3;\n", out);

    EXPECT_EQ(programOutput.str(), "9\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, StringConcatenation_PrintsConcatenatedString) {
    std::ostringstream out;
    run("print \"Hello, \" + \"CodeFab!\";\n", out);

    EXPECT_EQ(programOutput.str(), "Hello, CodeFab!\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, ComparisonExpression_PrintsTrue) {
    std::ostringstream out;
    run("print 1 < 2;\n", out);

    EXPECT_EQ(programOutput.str(), "true\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, BooleanLiteral_PrintsTrue) {
    std::ostringstream out;
    run("print true;\n", out);

    EXPECT_EQ(programOutput.str(), "true\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, BooleanLiteral_PrintsFalse) {
    std::ostringstream out;
    run("print false;\n", out);

    EXPECT_EQ(programOutput.str(), "false\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, SubtractionIsLeftAssociative_PrintsThree) {
    std::ostringstream out;
    run("print 10 - 4 - 3;\n", out);

    EXPECT_EQ(programOutput.str(), "3\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, DivisionIsLeftAssociative_PrintsTwo) {
    std::ostringstream out;
    run("print 8 / 2 / 2;\n", out);

    EXPECT_EQ(programOutput.str(), "2\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, UnaryMinusThenAddition_PrintsNegativeOne) {
    std::ostringstream out;
    run("print -3 + 2;\n", out);

    EXPECT_EQ(programOutput.str(), "-1\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, ComparisonExpression_PrintsFalse) {
    std::ostringstream out;
    run("print 3 > 5;\n", out);

    EXPECT_EQ(programOutput.str(), "false\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, IntegerLiteral_PrintsWithoutDecimalPart) {
    std::ostringstream out;
    run("print 5;\n", out);

    EXPECT_EQ(programOutput.str(), "5\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, WholeNumberFloatLiteral_PrintsWithoutTrailingZero) {
    std::ostringstream out;
    run("print 5.0;\n", out);

    EXPECT_EQ(programOutput.str(), "5\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, DecimalLiteral_PrintsWithDecimalPart) {
    std::ostringstream out;
    run("print 3.14;\n", out);

    EXPECT_EQ(programOutput.str(), "3.14\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}


// --- 1-1. '\' 줄 이음 시나리오: 실제 파이프라인으로 여러 줄에 걸쳐 입력해도 정상 처리된다 ---

TEST_F(RunPromptShellIntegrationTest, LineEndingWithBackslash_JoinsWithNextLineAndPrintsThree) {
    std::ostringstream out;
    run("print 1 + \\\n2;\n", out);

    EXPECT_EQ(programOutput.str(), "3\n");
    EXPECT_EQ(out.str(), ">>> ... >>> ");
}

TEST_F(RunPromptShellIntegrationTest, LineEndingWithBackslash_AcrossThreeLinesJoinsAllAndPrintsInner) {
    std::ostringstream out;
    run("{ var x = \\\n\"inner\"; \\\nprint x; }\n", out);

    EXPECT_EQ(programOutput.str(), "inner\n");
    EXPECT_EQ(out.str(), ">>> ... ... >>> ");
}

// --- 2-1. 구문 에러 시나리오: 실제 Assembler가 std::invalid_argument를 던지는 경우 ---
// Assembler가 던지는 메시지에는 실제 Tokenizer가 항상 덧붙이는 EOF 토큰 등의 영향으로
// "(near '...' at line N)" 접미사가 붙을 수 있어, 핵심 문구만 부분 일치로 검증한다.

TEST_F(RunPromptShellIntegrationTest, MissingSemicolon_ReportsSyntaxError) {
    std::ostringstream out;
    run("print 1 + 2\n", out);

    EXPECT_EQ(programOutput.str(), "");  // Executor까지 도달하지 않았다.
    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "), HasSubstr("Expect ';' after value."),
                                  EndsWith(">>> ")));
}

TEST_F(RunPromptShellIntegrationTest, MissingClosingParen_ReportsSyntaxError) {
    std::ostringstream out;
    // 참고: gist 원문 "print (1 + 2;"은 괄호 개수가 안 맞아 실제 Tokenizer가
    // "입력이 아직 완결되지 않음"으로 판단해 계속 입력을 기다리게 된다 (Assembler까지
    // 도달하지 못함). 그래서 괄호 개수는 맞지만 ')' 자리에 다른 토큰이 오는 문장으로
    // 대체해 동일한 "Expect ')' after expression." 오류 경로를 재현한다.
    run("print (1 + 2 3);\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "), HasSubstr("Expect ')' after expression."),
                                  EndsWith(">>> ")));
}

TEST_F(RunPromptShellIntegrationTest, InvalidAssignmentTarget_ReportsSyntaxError) {
    std::ostringstream out;
    run("a + b = 3;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "), HasSubstr("Invalid assignment target."),
                                  EndsWith(">>> ")));
}

TEST_F(RunPromptShellIntegrationTest, UnexpectedTokenInExpression_ReportsSyntaxError) {
    std::ostringstream out;
    run("print * 5;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "), HasSubstr("Expect expression."),
                                  EndsWith(">>> ")));
}

TEST_F(RunPromptShellIntegrationTest, MissingClosingParenInSubExpression_ReportsSyntaxError) {
    std::ostringstream out;
    run("print 1 * (2 + 3;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "), HasSubstr("Expect ')' after expression."),
                                  EndsWith(">>> ")));
}

TEST_F(RunPromptShellIntegrationTest, MissingClosingBrace_ReportsSyntaxError) {
    std::ostringstream out;
    run("{ var x = 1;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "), HasSubstr("Expect '}' after block."),
                                  EndsWith(">>> ")));
}

// --- 2-2. Checker 정적 에러 시나리오 ---
// 실제 Checker는 의미 오류를 찾으면 줄 번호를 메시지에 직접 담아 CheckerError를
// throw하고, Shell은 이를 catch(const std::exception&)로 잡아 메시지를 그대로
// 출력한다 (상세 메시지에 대한 단위 테스트는 CheckerTest.cpp도 별도로 검증한다).
// 참고: gist 원문은 이 두 케이스에 영어 메시지를 기대하지만, Checker가 실제로
// 던지는 메시지는 한글이라 그 문구가 그대로 노출된다.
TEST_F(RunPromptShellIntegrationTest, ReadLocalVariableInOwnInitializer_FailsCheckWithoutExecuting) {
    std::ostringstream out;
    run("{ var a = a; }\n", out);

    EXPECT_EQ(programOutput.str(), "");  // Executor가 호출되지 않았다.
    EXPECT_EQ(out.str(), ">>> [1번째 줄] 자신의 초기화식에서 지역변수를 읽을 수 없습니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, DuplicateLocalDeclaration_FailsCheckWithoutExecuting) {
    std::ostringstream out;
    run("{ var a = \"hi\"; var a = 3; }\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> [1번째 줄] 'a'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, UndefinedVariableReference_FailsCheckWithoutExecuting) {
    std::ostringstream out;
    run("print notDefined;\n", out);

    // 참고: gist는 이 케이스를 "런타임 에러"로 분류하지만, 실제로는 Checker의
    // "선언되지 않은 변수" 규칙(checkIdentifier의 isDeclaredInAnyScope 검사)이
    // 이미 이 시점에 잡아내서 Executor까지 도달하지 않는다. Executor.cpp에도
    // 미정의 변수 참조 시 ExecutorError를 던지는 코드가 있지만, Checker가 먼저
    // 막아서 이 경로에서는 노출되지 않는다.
    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> [1번째 줄] 'notDefined'에러: 선언되지 않은 변수입니다.\n>>> ");
}

// --- 2-3. 실행 중(런타임) 에러 시나리오 ---
// 실제 Executor는 타입이 안 맞는 산술 연산에서 ExecutorError를 던지고,
// 실제로 구현되어 있는 메시지는 gist가 기대하는 다듬어진 영어 문구가 아니라
// 한글 문구("타입 오류: ...")다. ExecutorError는 줄 번호를 담지 않으므로
// "[N번째 줄]" 접두사 없이 메시지만 출력된다.

TEST_F(RunPromptShellIntegrationTest, MixedTypeAddition_ReportsTypeMismatchError) {
    std::ostringstream out;
    run("print 1 + \"HI\";\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> 타입 오류: number + string\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, UnaryMinusOnNonNumber_ReportsOperandTypeError) {
    std::ostringstream out;
    run("print -\"FabCoding\";\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> 타입 오류: -string\n>>> ");
}

// --- 3. 변수 선언 / 할당 / 블록 스코프 / shadowing ---
// 아래 시나리오들은 gist 원문처럼 선언과 사용을 서로 다른 REPL 줄로 나눠 테스트한다.
// Checker는 Executor의 Environment와 마찬가지로 세션 전체에 걸쳐 유지되는 전역
// 스코프를 갖고 있어서(checker.cpp의 Checker::Checker() 참고), 한 줄에서 선언한
// 변수를 다음 줄에서도 "선언된 변수"로 인식한다.

TEST_F(RunPromptShellIntegrationTest, VariableDeclarationAndUse_PrintsSum) {
    std::ostringstream out;
    run("var a = 10;\nvar b = 20;\nprint a + b;\n", out);

    EXPECT_EQ(programOutput.str(), "30\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, Reassignment_PrintsUpdatedValue) {
    std::ostringstream out;
    run("var a = 10;\na = a + 5;\nprint a;\n", out);

    EXPECT_EQ(programOutput.str(), "15\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, BlockScopeShadowing_InnerHidesOuterButOuterSurvivesBlock) {
    std::ostringstream out;
    run("var x = \"global\";\n{ var x = \"inner\"; print x; }\nprint x;\n", out);

    EXPECT_EQ(programOutput.str(), "inner\nglobal\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, MutatingOuterVariableFromInnerBlock_UpdatesOuter) {
    std::ostringstream out;
    run("var count = 0;\n{ count = count + 1; }\nprint count;\n", out);

    EXPECT_EQ(programOutput.str(), "1\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, NestedScopeResolution_ReferencesOuterAndInnerVariables) {
    std::ostringstream out;
    run("var outer = \"A\";\n{ var inner = \"B\"; { print outer + inner; } }\n", out);

    EXPECT_EQ(programOutput.str(), "AB\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> ");
}

// --- 4. 제어 흐름: 블록 스코프 / if-else / for ---

TEST_F(RunPromptShellIntegrationTest, BlockScope_PrintsInner) {
    std::ostringstream out;
    run("{ var x = \"inner\"; print x; }\n", out);

    EXPECT_EQ(programOutput.str(), "inner\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, IfElse_PrintsKfc) {
    std::ostringstream out;
    run("if (false) print \"no\"; else print \"kfc\";\n", out);

    EXPECT_EQ(programOutput.str(), "kfc\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, DanglingElse_BindsToNearestIf) {
    std::ostringstream out;
    run("if (true) { if (false) print \"kfc\"; else print \"bbq\"; }\n", out);

    EXPECT_EQ(programOutput.str(), "bbq\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, ForLoop_WithVarInitializer_PrintsZeroOneTwo) {
    std::ostringstream out;
    run("for (var j = 0; j < 3; j = j + 1) { print j; }\n", out);

    EXPECT_EQ(programOutput.str(), "0\n1\n2\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

// --- 5. 논리 연산자(and/or) ---
// PDF 스펙(Assembly Unit 핵심원리 - Expr 예시)이 요구하는 Logical Expr(and/or)은
// 아직 Tokenizer/Assembler/Executor 어디에도 구현되어 있지 않다. 이 테스트들은
// 현재 시점에는 FAIL 하도록 의도적으로 추가한 것으로, 미구현 기능을 추적하기 위한
// 표시용(TDD red) 테스트다. and/or를 구현하면 이 테스트들이 통과해야 한다.
TEST_F(RunPromptShellIntegrationTest, LogicalAnd_PrintsFalseWhenLeftOperandIsFalse) {
    std::ostringstream out;
    run("print false and true;\n", out);

    EXPECT_EQ(programOutput.str(), "false\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, LogicalOr_PrintsTrueWhenLeftOperandIsFalseButRightIsTrue) {
    std::ostringstream out;
    run("print false or true;\n", out);

    EXPECT_EQ(programOutput.str(), "true\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

// --- 6. Checker의 if/for 내부 검사 ---
// Checker::checkStatement(Checker.cpp)는 이제 IfStatement/ForStatement에 대한 분기도
// 갖고 있어서(checkIf/checkFor), if/for 문 안에 중첩된 블록의 의미 오류(중복 선언,
// 자기 참조)도 최상위 블록과 동일하게 실행 전에 잡아낸다.
// (과거에는 이 분기가 없어 if/for 안의 오류가 조용히 통과됐다 - CheckerTest.cpp의
// DuplicateDeclarationInsideIfBlockReportsError 등 단위 테스트가 그 회귀를 방지한다.)
TEST_F(RunPromptShellIntegrationTest, DuplicateDeclarationInsideIfBlock_FailsCheckWithoutExecuting) {
    std::ostringstream out;
    run("if (true) { var a = \"hi\"; var a = 3; }\n", out);

    EXPECT_EQ(programOutput.str(), "");  // Executor가 호출되지 않았다.
    EXPECT_EQ(out.str(), ">>> [1번째 줄] 'a'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, SelfReferenceInsideIfBlock_FailsCheckWithoutExecuting) {
    std::ostringstream out;
    run("if (true) { var a = a; }\n", out);

    EXPECT_EQ(programOutput.str(), "");  // Executor가 호출되지 않았다.
    EXPECT_EQ(out.str(), ">>> [1번째 줄] 자신의 초기화식에서 지역변수를 읽을 수 없습니다.\n>>> ");
}
