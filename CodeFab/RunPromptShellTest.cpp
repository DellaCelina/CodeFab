#include "RunPromptShell.h"

#include <sstream>
#include <string>
#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "Assembler.h"
#include "AssemblerInterface.h"
#include "CheckerInterface.h"
#include "ExecuteInterface.h"
#include "ShellErrors.h"
#include "SyntaxTree.h"
#include "Token.h"
#include "TokenizeInterface.h"
#include "Tokenizer.h"

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
        .WillOnce(Throw(IncompleteInputError(1, "입력이 완결되지 않았습니다.")));

    std::ostringstream out;
    run("if (a > 0) {\n", out);

    EXPECT_EQ(out.str(), ">>> ... ");
}

TEST_F(RunPromptShellTest, MultilineBlock_CompletesWhenBraceClosesAndTokenizerStopsReportingIncomplete) {
    InSequence seq;
    EXPECT_CALL(tokenizer, tokenize(std::string("if (a > 0) {")))
        .WillOnce(Throw(IncompleteInputError(1, "입력이 완결되지 않았습니다.")));
    EXPECT_CALL(tokenizer, tokenize(std::string("if (a > 0) {\nprint a;")))
        .WillOnce(Throw(IncompleteInputError(1, "입력이 완결되지 않았습니다.")));
    EXPECT_CALL(tokenizer, tokenize(std::string("if (a > 0) {\nprint a;\n}")))
        .WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    run("if (a > 0) {\nprint a;\n}\n", out);

    EXPECT_EQ(out.str(), ">>> ... ... >>> ");
}

TEST_F(RunPromptShellTest, TokenizeError_IsReportedAndRestOfPipelineIsSkipped) {
    EXPECT_CALL(tokenizer, tokenize(_))
        .WillOnce(Throw(AssemblyError(1, "인식할 수 없는 문자입니다.")));
    EXPECT_CALL(assembler, assemble(_)).Times(0);
    EXPECT_CALL(checker, check(_)).Times(0);
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    run("var a = @;\n", out);

    EXPECT_EQ(out.str(), ">>> [1번째 줄] 인식할 수 없는 문자입니다.\n>>> ");
}

TEST_F(RunPromptShellTest, AssemblyError_IsReportedAndCheckerExecutorAreSkipped) {
    EXPECT_CALL(tokenizer, tokenize(_)).WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_))
        .WillOnce(Throw(AssemblyError(1, "'+' 다음에 피연산자가 필요합니다.")));
    EXPECT_CALL(checker, check(_)).Times(0);
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    run("var a = 3 + ;\n", out);

    EXPECT_EQ(out.str(), ">>> [1번째 줄] '+' 다음에 피연산자가 필요합니다.\n>>> ");
}

TEST_F(RunPromptShellTest, CheckErrorThrown_IsReportedAndExecutorIsSkipped) {
    EXPECT_CALL(tokenizer, tokenize(_)).WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_))
        .WillOnce(Throw(CheckerError(2, "'a'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.")));
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
    EXPECT_CALL(executor, execute(_))
        .WillOnce(Throw(RuntimeCodeFabError(1, "0으로 나눈 오류")))
        .WillOnce(Return());

    std::ostringstream out;
    run("a = 3 / 0;\nprint a;\n", out);

    EXPECT_EQ(out.str(), ">>> [1번째 줄] 0으로 나눈 오류\n>>> >>> ");
}

TEST_F(RunPromptShellTest, ErrorOnOneLine_DoesNotPreventNextLineFromRunning) {
    EXPECT_CALL(tokenizer, tokenize(std::string("x = 5;")))
        .WillOnce(Throw(RuntimeCodeFabError(1, "미정의된 변수 'x'")));
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
// Tokenizer/Assembler는 실제 구현을 사용한다. Checker/Executor는 아직 구현이 없어
// (NotImplemented 스텁만 존재) 계속 Mock으로 대체한다.
// 따라서 "정상 동작" 시나리오는 실제 언어 결과값(예: 7, true 등)까지는 검증하지 못하고,
// 실제 Tokenizer+Assembler가 해당 소스를 에러 없이 파싱해 Checker/Executor까지
// 도달하는지만 확인한다. 반면 "구문 에러" 시나리오는 이제 실제 Assembler가 던지는
// 예외를 그대로 검증한다.
// 실제 Checker/Executor가 병합되면 "정상 동작" 시나리오도 실제 출력/의미 검증으로
// 교체해야 한다.
// ============================================================================
class RunPromptShellIntegrationTest : public ::testing::Test {
protected:
    Tokenizer tokenizer;
    Assembler assembler;
    NiceMock<MockChecker> checker;
    NiceMock<MockExecutor> executor;

    RunPromptShell shell{tokenizer, assembler, checker, executor};

    void run(const std::string& input, std::ostringstream& out) {
        std::istringstream in(input);
        shell.run(in, out);
    }
};

// --- 1. 정상 동작 시나리오: 실제 Tokenizer+Assembler가 에러 없이 Checker/Executor까지 도달하는지 확인 ---

TEST_F(RunPromptShellIntegrationTest, ArithmeticPrecedence_ReachesExecutor) {
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    run("print 1 + 2 * 3;\n", out);  // expect(실제 언어 동작): 7
}

TEST_F(RunPromptShellIntegrationTest, ParenthesizedExpression_ReachesExecutor) {
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    run("print (1 + 2) * 3;\n", out);  // expect(실제 언어 동작): 9
}

TEST_F(RunPromptShellIntegrationTest, StringConcatenation_ReachesExecutor) {
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    run("print \"Hello, \" + \"CodeFab!\";\n", out);  // expect(실제 언어 동작): Hello, CodeFab!
}

TEST_F(RunPromptShellIntegrationTest, ComparisonExpression_ReachesExecutor) {
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    run("print 1 < 2;\n", out);  // expect(실제 언어 동작): true
}

TEST_F(RunPromptShellIntegrationTest, BooleanLiteral_ReachesExecutor) {
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    run("print true;\n", out);  // expect(실제 언어 동작): true
}

TEST_F(RunPromptShellIntegrationTest, VariableDeclarationAndBlockScope_ReachesExecutor) {
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    run("{ var x = \"inner\"; print x; }\n", out);  // expect(실제 언어 동작): inner
}

TEST_F(RunPromptShellIntegrationTest, IfElse_ReachesExecutor) {
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    run("if (false) print \"no\"; else print \"kfc\";\n", out);  // expect(실제 언어 동작): kfc
}

TEST_F(RunPromptShellIntegrationTest, ForLoop_ReachesExecutor) {
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    // 참고: gist 원문은 `for (var j = 0; ...)`이지만, 현재 Assembler의 for문 문법은
    // 초기화절에 expression만 허용하고 var 선언은 지원하지 않는다 (assembler.cpp
    // parseForStatement 참고). 그래서 대입식(j = 0)으로 바꿔 실제로 파싱 가능한 형태로 둔다.
    run("for (j = 0; j < 3; j = j + 1) { print j; }\n", out);  // expect(실제 언어 동작): 0, 1, 2
}

// --- 2-1. 구문 에러 시나리오: 실제 Assembler가 std::invalid_argument를 던지는 경우 ---
// Assembler가 던지는 메시지에는 실제 Tokenizer가 항상 덧붙이는 EOF 토큰 등의 영향으로
// "(near '...' at line N)" 접미사가 붙을 수 있어, 핵심 문구만 부분 일치로 검증한다.

TEST_F(RunPromptShellIntegrationTest, MissingSemicolon_ReportsSyntaxError) {
    EXPECT_CALL(checker, check(_)).Times(0);
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    run("print 1 + 2\n", out);

    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "), HasSubstr("Expect ';' after value."),
                                  EndsWith(">>> ")));
}

TEST_F(RunPromptShellIntegrationTest, MissingClosingParen_ReportsSyntaxError) {
    EXPECT_CALL(checker, check(_)).Times(0);
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    // 참고: gist 원문 "print (1 + 2;"은 괄호 개수가 안 맞아 실제 Tokenizer가
    // "입력이 아직 완결되지 않음"으로 판단해 계속 입력을 기다리게 된다 (Assembler까지
    // 도달하지 못함). 그래서 괄호 개수는 맞지만 ')' 자리에 다른 토큰이 오는 문장으로
    // 대체해 동일한 "Expect ')' after expression." 오류 경로를 재현한다.
    run("print (1 + 2 3);\n", out);

    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "), HasSubstr("Expect ')' after expression."),
                                  EndsWith(">>> ")));
}

TEST_F(RunPromptShellIntegrationTest, InvalidAssignmentTarget_ReportsSyntaxError) {
    EXPECT_CALL(checker, check(_)).Times(0);
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    run("a + b = 3;\n", out);

    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "), HasSubstr("Invalid assignment target."),
                                  EndsWith(">>> ")));
}

TEST_F(RunPromptShellIntegrationTest, UnexpectedTokenInExpression_ReportsSyntaxError) {
    EXPECT_CALL(checker, check(_)).Times(0);
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    run("print * 5;\n", out);

    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "), HasSubstr("Expect expression."),
                                  EndsWith(">>> ")));
}

// --- 2-2. Checker 정적 에러 시나리오 (Checker는 여전히 Mock) ---

TEST_F(RunPromptShellIntegrationTest, ReadLocalVariableInOwnInitializer_ReportsCheckError) {
    EXPECT_CALL(checker, check(_))
        .WillOnce(Throw(CheckError(1, "Can't read local variable in initializer.")));
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    run("{ var a = a; }\n", out);

    EXPECT_EQ(out.str(), ">>> [1번째 줄] Can't read local variable in initializer.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, DuplicateLocalDeclaration_ReportsCheckError) {
    EXPECT_CALL(checker, check(_))
        .WillOnce(Throw(CheckError(1, "Already a variable with this name in this scope.")));
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    run("{ var a = \"hi\"; var a = 3; }\n", out);

    EXPECT_EQ(out.str(),
              ">>> [1번째 줄] Already a variable with this name in this scope.\n>>> ");
}

// --- 2-3. 실행 중(런타임) 에러 시나리오 (Executor는 여전히 Mock) ---

TEST_F(RunPromptShellIntegrationTest, UndefinedVariableReference_ReportsRuntimeError) {
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_))
        .WillOnce(Throw(RuntimeCodeFabError(1, "Undefined variable 'notDefined'.")));

    std::ostringstream out;
    run("print notDefined;\n", out);

    EXPECT_EQ(out.str(), ">>> [1번째 줄] Undefined variable 'notDefined'.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, MixedTypeAddition_ReportsRuntimeError) {
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_))
        .WillOnce(Throw(RuntimeCodeFabError(1, "Operands must be two numbers or two strings.")));

    std::ostringstream out;
    run("print 1 + \"HI\";\n", out);

    EXPECT_EQ(out.str(),
              ">>> [1번째 줄] Operands must be two numbers or two strings.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, UnaryMinusOnNonNumber_ReportsRuntimeError) {
    EXPECT_CALL(checker, check(_)).WillOnce(Return(true));
    EXPECT_CALL(executor, execute(_))
        .WillOnce(Throw(RuntimeCodeFabError(1, "Operand must be a number.")));

    std::ostringstream out;
    run("print -\"FabCoding\";\n", out);

    EXPECT_EQ(out.str(), ">>> [1번째 줄] Operand must be a number.\n>>> ");
}
