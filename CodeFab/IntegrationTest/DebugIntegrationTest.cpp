#include "../Shell/RunPromptShell.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "../Assembler/Assembler.h"
#include "../Assembler/AssemblerInterface.h"
#include "../Assembler/FileSourceReader.h"
#include "../Assembler/SyntaxTree.h"
#include "../Checker/Checker.h"
#include "../Checker/CheckerInterface.h"
#include "../Executor/ExecuteInterface.h"
#include "../Executor/Executor.h"
#include "../Tokenizer/Token.h"
#include "../Tokenizer/TokenizeInterface.h"
#include "../Tokenizer/Tokenizer.h"

using ::testing::AllOf;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::StartsWith;

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
    Checker checker;

    RunPromptShell shell{tokenizer, assembler, checker, executor};

    // import 통합 테스트용: FileSourceReader가 실제 파일 시스템을 읽으므로,
    // 임시 디렉터리에 라이브러리 파일을 써두고 테스트가 끝나면 지운다
    // (FileRunModeTest.cpp의 임시 파일 패턴과 동일).
    std::vector<std::filesystem::path> tempFiles_;

    void run(const std::string& input, std::ostringstream& out) {
        std::istringstream in(input);
        shell.run(in, out);
    }

    std::string writeTempFile(const std::string& filename, const std::string& content) {
        std::filesystem::path path = std::filesystem::temp_directory_path() / filename;
        std::ofstream file(path);
        file << content;
        file.close();
        tempFiles_.push_back(path);
        return path.string();
    }

    void TearDown() override {
        for (const auto& path : tempFiles_) {
            std::filesystem::remove(path);
        }
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

// if 블록도 BlockStatement와 마찬가지로 자기 스코프를 갖는지 확인한다: if 블록
// 안에서 선언한 지역 변수(a)는 블록을 빠져나오면 사라지고, if 문 이전에 선언한
// 바깥 변수(g)는 블록 실행 여부와 무관하게 계속 접근 가능해야 한다.
TEST_F(RunPromptShellIntegrationTest, IfBlockScope_InnerVariableIsScopedAndOuterVariableRemainsAccessible) {
    std::ostringstream out;
    run("var g = 1;\nif (true) { var a = 2; print a; }\nprint g;\n", out);

    EXPECT_EQ(programOutput.str(), "2\n1\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

// 조건이 false면 if 블록 자체가 실행되지 않으므로, 블록 안의 print(2)는 전혀
// 출력되지 않고 바깥 변수(g)만 출력돼야 한다.
TEST_F(RunPromptShellIntegrationTest, IfBlockScope_ConditionFalse_SkipsBlockAndDoesNotPrintInnerValue) {
    std::ostringstream out;
    run("var g = 1;\nif (false) { var a = 2; print a; }\nprint g;\n", out);

    EXPECT_EQ(programOutput.str(), "1\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

// --- 5. 논리 연산자(and/or) ---
// Tokenizer/Assembler/Executor 모두 and/or를 구현하고 있다 (Executor.cpp의
// AndExpression/OrExpression 핸들러, 둘 다 단락 평가). 아래는 실제 파이프라인으로
// ExecutorTest.cpp의 and/or 단위 테스트가 다루는 케이스를 재현한다.
TEST_F(RunPromptShellIntegrationTest, LogicalAnd_PrintsFalseWhenLeftOperandIsFalse) {
    std::ostringstream out;
    run("print false and true;\n", out);

    EXPECT_EQ(programOutput.str(), "false\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, LogicalAnd_PrintsTrueWhenBothOperandsAreTrue) {
    std::ostringstream out;
    run("print true and true;\n", out);

    EXPECT_EQ(programOutput.str(), "true\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, LogicalOr_PrintsTrueWhenLeftOperandIsFalseButRightIsTrue) {
    std::ostringstream out;
    run("print false or true;\n", out);

    EXPECT_EQ(programOutput.str(), "true\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, LogicalOr_PrintsFalseWhenBothOperandsAreFalse) {
    std::ostringstream out;
    run("print false or false;\n", out);

    EXPECT_EQ(programOutput.str(), "false\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

// --- 5-1. % (나머지) 연산자 ---
// ExecutorTest.cpp의 Evaluate_ModExpression_ReturnsRemainder /
// Evaluate_ModExpression_ThrowsOnZero에 대응하는 통합 테스트다.
TEST_F(RunPromptShellIntegrationTest, ModExpression_PrintsRemainder) {
    std::ostringstream out;
    run("print 10 % 3;\n", out);

    EXPECT_EQ(programOutput.str(), "1\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, ModByZero_ReportsRuntimeError) {
    std::ostringstream out;
    run("print 10 % 0;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> 0으로 나눌 수 없습니다\n>>> ");
}

// TODO.md #8 잔여 갭: ExecutorTest.cpp는 단위 테스트 수준에서 short-circuit을
// 확인하지만, end-to-end로 우항이 실제로 평가되지 않는지(부작용이 남지 않는지)
// 확인하는 통합 테스트는 없었다. bump()가 호출되면 c가 증가하므로, 좌항이 false인
// and는 우항을 평가하지 않아 c가 그대로 0으로 남아야 한다.
TEST_F(RunPromptShellIntegrationTest, LogicalAnd_ShortCircuit_DoesNotEvaluateRightOperand) {
    std::ostringstream out;
    run("var c = 0;\n"
        "Func bump() { c = c + 1; return true; }\n"
        "var x = false and bump();\n"
        "print c;\n", out);

    EXPECT_EQ(programOutput.str(), "0\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> >>> ");
}

// 대칭 시나리오: or의 좌항이 true면 우항을 평가하지 않는다.
TEST_F(RunPromptShellIntegrationTest, LogicalOr_ShortCircuit_DoesNotEvaluateRightOperand) {
    std::ostringstream out;
    run("var c = 0;\n"
        "Func bump() { c = c + 1; return true; }\n"
        "var x = true or bump();\n"
        "print c;\n", out);

    EXPECT_EQ(programOutput.str(), "0\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> >>> ");
}

// TODO.md #8 잔여 갭: %가 */와 같은 우선순위·좌결합으로 파싱/실행되는지 복합식으로
// 확인한다(Assembler.cpp의 kDefaultOperatorPriority가 PERCENT를 STAR/SLASH와 같은
// 그룹에 두고 있음 - AssemblerTest.cpp에 파싱 단위 테스트는 있지만 end-to-end 값
// 확인은 없었다). 2 * 3 % 4는 좌결합이면 (2 * 3) % 4 = 6 % 4 = 2.
TEST_F(RunPromptShellIntegrationTest, ModMixedWithMultiplication_LeftAssociative_PrintsTwo) {
    std::ostringstream out;
    run("print 2 * 3 % 4;\n", out);

    EXPECT_EQ(programOutput.str(), "2\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

// %가 연속으로 두 번 적용되는 경우: (1 - 2*3*4*5/6 + 7 + 8 + 9) % 1000 % 30.
// 안쪽은 */가 좌결합이므로 2*3*4*5/6 = 20 -> 1 - 20 + 7 + 8 + 9 = 5,
// 이어서 5 % 1000 % 30도 좌결합이므로 (5 % 1000) % 30 = 5.
TEST_F(RunPromptShellIntegrationTest, ChainedModExpression_PrintsFive) {
    std::ostringstream out;
    run("print (1 - 2 * 3 * 4 * 5 / 6 + 7 + 8 + 9) % 1000 % 30;\n", out);

    EXPECT_EQ(programOutput.str(), "5\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

// --- 5-2. 비교 연산자 확장(==, !=, <=, >=) 및 논리 부정(!) ---
// ExecutorTest.cpp가 evaluate() 단위로 검증하는 EqualExpression/NotEqualExpression/
// LessEqualExpression/GreaterEqualExpression/NotExpression을 실제 파이프라인으로
// 재현한다.
TEST_F(RunPromptShellIntegrationTest, EqualExpression_PrintsTrue) {
    std::ostringstream out;
    run("print 3 == 3;\n", out);

    EXPECT_EQ(programOutput.str(), "true\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, NotEqualExpression_PrintsTrue) {
    std::ostringstream out;
    run("print 3 != 5;\n", out);

    EXPECT_EQ(programOutput.str(), "true\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, LessEqualExpression_PrintsTrue) {
    std::ostringstream out;
    run("print 3 <= 3;\n", out);

    EXPECT_EQ(programOutput.str(), "true\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, GreaterEqualExpression_PrintsTrue) {
    std::ostringstream out;
    run("print 5 >= 3;\n", out);

    EXPECT_EQ(programOutput.str(), "true\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, NotExpression_NegatesTrueLiteral) {
    std::ostringstream out;
    run("print !true;\n", out);

    EXPECT_EQ(programOutput.str(), "false\n");
    EXPECT_EQ(out.str(), ">>> >>> ");
}

// --- 5-3. 0으로 나눔 / 산술 연산자 타입 오류 ---
// ExecutorTest.cpp의 Evaluate_DivideByZero_ThrowsRuntimeError,
// Evaluate_SubStringAndNumber_ThrowsRuntimeError,
// Evaluate_AddBooleanAndNumber_ThrowsRuntimeError,
// Evaluate_MultStringAndBoolean_ThrowsRuntimeError에 대응하는 통합 테스트다.
TEST_F(RunPromptShellIntegrationTest, DivideByZero_ReportsRuntimeError) {
    std::ostringstream out;
    run("print 3 / 0;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> 0으로 나눌 수 없습니다\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, SubStringAndNumber_ReportsTypeMismatchError) {
    std::ostringstream out;
    run("print \"hello\" - 3;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> 타입 오류: string - number\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, AddBooleanAndNumber_ReportsTypeMismatchError) {
    std::ostringstream out;
    run("print true + 3;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> 타입 오류: boolean + number\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, MultStringAndBoolean_ReportsTypeMismatchError) {
    std::ostringstream out;
    run("print \"hello\" * true;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> 타입 오류: string * boolean\n>>> ");
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

TEST_F(RunPromptShellIntegrationTest, FuncDeclaredThenCalledOnNextLine_ReturnsCorrectValue) {
    std::ostringstream out;
    run("Func add(a, b) { return a + b; }\nprint add(3, 7);\n", out);

    EXPECT_EQ(programOutput.str(), "10\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, ClassMethodCalledAfterInstantiationOnSeparateLines_ReturnsCorrectValue) {
    std::ostringstream out;
    run("Class Counter { init(n) { This.n = n; } get() { return This.n; } }\n"
        "var c = Counter(42);\n"
        "print c.get();\n", out);

    EXPECT_EQ(programOutput.str(), "42\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

// --- 7. 정적 배열(Array) ---
// ExecutorArrayTest.cpp가 evaluate()/execute() 단위로 검증하는 생성/인덱스
// 읽기·쓰기/런타임 에러 케이스를 실제 파이프라인으로 재현한다.
TEST_F(RunPromptShellIntegrationTest, ArrayCreation_FillsWithNil) {
    std::ostringstream out;
    run("var arr = Array(3);\nprint arr[0];\n", out);

    EXPECT_EQ(programOutput.str(), "nil\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, IndexWrite_ThenReadReturnsStoredValue) {
    std::ostringstream out;
    run("var arr = Array(5);\narr[0] = 3;\nprint arr[0];\n", out);

    EXPECT_EQ(programOutput.str(), "3\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, ArraySize_NotANumber_ReportsRuntimeError) {
    std::ostringstream out;
    run("var arr = Array(\"5\");\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> 배열의 사이즈는 반드시 number여야 합니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, IndexOnNonArray_ReportsRuntimeError) {
    std::ostringstream out;
    run("var x = 1;\nprint x[0];\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> >>> index 접근은 오직 배열만 지원합니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, IndexNotANumber_ReportsRuntimeError) {
    std::ostringstream out;
    run("var arr = Array(3);\nprint arr[\"zero\"];\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> >>> 인덱스는 반드시 숫자여야 합니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, IndexOutOfRange_ReportsRuntimeError) {
    std::ostringstream out;
    run("var arr = Array(3);\nprint arr[3];\n", out);  // 유효 범위는 0..2

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> >>> 배열 인덱스 범위를 벗어났습니다.\n>>> ");
}

// 1일차/3일차 슬라이드의 정적 배열 예시(var i = 2; arr[i - 1] = 7;)처럼, 인덱스
// 자리에 리터럴이 아닌 계산식(변수 - 리터럴)이 와도 정상 동작하는지 확인한다.
TEST_F(RunPromptShellIntegrationTest, IndexWriteWithComputedExpression_WritesAndReadsCorrectSlot) {
    std::ostringstream out;
    run("var arr = Array(3);\nvar i = 2;\narr[i - 1] = 7;\nprint arr[1];\n", out);

    EXPECT_EQ(programOutput.str(), "7\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, DeclareAndCall_ReturnsSum) {
    std::ostringstream out;
    run("Func add(a, b) { return a + b; }\nprint add(3, 5);\n", out);

    EXPECT_EQ(programOutput.str(), "8\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, RecursiveCall_ComputesFactorial) {
    std::ostringstream out;
    run("Func fact(n) { if (n <= 1) { return 1; } return n * fact(n - 1); }\nprint fact(5);\n", out);

    EXPECT_EQ(programOutput.str(), "120\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, CallWithWrongArgumentCount_ReportsRuntimeError) {
    std::ostringstream out;
    run("Func oneArg(a) { }\nprint oneArg();\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> >>> 'oneArg' 호출에는 인자 1개가 필요합니다 (전달된 인자: 0개)\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, CallingNonCallableValue_ReportsRuntimeError) {
    std::ostringstream out;
    run("var x = 1;\nprint x();\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> >>> 호출할 수 없는 대상입니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, CallWithoutReturn_YieldsNil) {
    std::ostringstream out;
    run("Func noop() { 1; }\nprint noop();\n", out);

    EXPECT_EQ(programOutput.str(), "nil\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> ");
}

// 슬라이드의 "return 처리" 예시: return; (인자 없음)은 nil을 반환한다.
// CallWithoutReturn_YieldsNil은 return문 자체가 없는 경우이고, 이 테스트는
// 명시적으로 값 없이 return; 을 작성한 경우를 구분해서 검증한다.
TEST_F(RunPromptShellIntegrationTest, ExplicitReturnWithoutValue_YieldsNil) {
    std::ostringstream out;
    run("Func noop() { return; }\nprint noop();\n", out);

    EXPECT_EQ(programOutput.str(), "nil\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> ");
}

// 함수/메서드 관련 오류 검사: 함수 외부에서 return 사용, 파라미터 이름 중복.
TEST_F(RunPromptShellIntegrationTest, ReturnOutsideFunction_ReportsCheckError) {
    std::ostringstream out;
    run("return 5;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> [1번째 줄] 함수(메서드) 밖에서 return을 사용할 수 없습니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, DuplicateParameterName_ReportsCheckError) {
    std::ostringstream out;
    run("Func foo(a, a) { }\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> [1번째 줄] 'foo'의 파라미터 이름 'a'이(가) 중복됩니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, InitSetsField_GetterMethodReturnsIt) {
    std::ostringstream out;
    run("Class Robot { init(name) { This.name = name; } getName() { return This.name; } }\n"
        "var r = Robot(\"ABC\");\nprint r.getName();\n", out);

    EXPECT_EQ(programOutput.str(), "ABC\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, FieldAccess_ReadsDirectlyWithoutMethod) {
    std::ostringstream out;
    run("Class Point { init(x) { This.x = x; } }\nvar p = Point(3);\nprint p.x;\n", out);

    EXPECT_EQ(programOutput.str(), "3\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, AccessingNonexistentField_ReportsRuntimeError) {
    std::ostringstream out;
    run("Class Empty { }\nvar e = Empty();\nprint e.missing;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    // 3개 문장(Class 선언/인스턴스화/print) 각각이 성공 여부와 무관하게 프롬프트를 하나씩
    // 남긴다 (RunPromptShell::run - 문장 처리 후 항상 kPrompt 출력, InstanceOf_SameClass_PrintsTrue
    // 등 다른 통합 테스트도 이 불변식을 그대로 사용한다).
    EXPECT_EQ(out.str(), ">>> >>> >>> 'missing' 필드가 존재하지 않습니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, CallingNonexistentMethod_ReportsRuntimeError) {
    std::ostringstream out;
    run("Class Empty { }\nvar e = Empty();\nprint e.missing();\n", out);

    EXPECT_EQ(programOutput.str(), "");
    // AccessingNonexistentField_ReportsRuntimeError와 동일한 이유로 프롬프트가 3개다.
    EXPECT_EQ(out.str(), ">>> >>> >>> 'missing' 메서드가 존재하지 않습니다.\n>>> ");
}

// 슬라이드의 "필드(Property) 읽기/쓰기" 예시: init 없이도 필드를 동적으로
// 새로 만들고(r.speed = 10), 기존 값을 읽어 갱신할 수 있는지(r.speed = r.speed + 5) 확인한다.
TEST_F(RunPromptShellIntegrationTest, FieldWriteReadAndUpdate_PrintsUpdatedValue) {
    std::ostringstream out;
    run("Class Robot { }\nvar r = Robot();\nr.speed = 10;\nr.speed = r.speed + 5;\nprint r.speed;\n", out);

    EXPECT_EQ(programOutput.str(), "15\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> >>> >>> ");
}

// 슬라이드의 "메서드와 this" 예시: 한 메서드(move)가 This로 필드를 갱신하고,
// 다른 메서드(report) 호출로 그 갱신된 값을 읽어오는지 확인한다.
TEST_F(RunPromptShellIntegrationTest, MethodUpdatesFieldViaThis_AnotherMethodReadsUpdatedValue) {
    std::ostringstream out;
    run("Class Robot { move(dist) { This.position = This.position + dist; } "
        "report() { print This.position; } }\n"
        "var r = Robot();\nr.position = 0;\nr.move(5);\nr.report();\n", out);

    EXPECT_EQ(programOutput.str(), "5\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> >>> >>> ");
}

// TODO.md #12: 함수 재귀 호출(RecursiveCall_ComputesFactorial)과 달리 메서드
// 재귀 호출은 이름 바인딩 경로가 다르다 - 함수는 호출 전에 스코프에 미리
// 등록되지만, 메서드는 매 호출마다 findMethod로 새로 찾고 this를 새로
// 바인딩해서 실행한다. This.fact(...) 형태의 메서드 재귀가 실제로 정상 동작하는지
// 확인한다.
TEST_F(RunPromptShellIntegrationTest, RecursiveMethodCall_ComputesFactorial) {
    std::ostringstream out;
    run("Class Calc { fact(n) { if (n <= 1) return 1; return n * This.fact(n - 1); } }\n"
        "var c = Calc();\n"
        "print c.fact(5);\n", out);

    EXPECT_EQ(programOutput.str(), "120\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

// 재귀가 깊어져도 매 호출마다 새로 바인딩되는 this가 항상 같은 인스턴스를
// 가리키는지 확인한다 - This.count로 필드를 누적시켜, 재귀가 끝난 뒤 필드 값이
// (별도의 인스턴스가 아니라) 호출자 자신의 필드 저장소에 누적됐는지로 검증한다.
TEST_F(RunPromptShellIntegrationTest, RecursiveMethodCall_ThisRemainsSameInstanceAcrossRecursion) {
    std::ostringstream out;
    run("Class Counter { bump(n) { This.count = This.count + 1; if (n > 1) { This.bump(n - 1); } } }\n"
        "var c = Counter();\n"
        "c.count = 0;\n"
        "c.bump(3);\n"
        "print c.count;\n", out);

    EXPECT_EQ(programOutput.str(), "3\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> >>> >>> ");
}

// 슬라이드의 클래스 관련 오류 검사: init 메서드는 항상 인스턴스를 반환해야 하므로
// return 값을 갖는 return문은 허용되지 않는다.
TEST_F(RunPromptShellIntegrationTest, InitWithReturnValue_ReportsCheckError) {
    std::ostringstream out;
    run("Class Robot { init() { return 5; } }\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> [1번째 줄] init 메서드는 값을 반환할 수 없습니다.\n>>> ");
}

// 슬라이드의 클래스 관련 오류 검사: 인스턴스가 아닌 대상에 필드를 대입하려는 경우.
TEST_F(RunPromptShellIntegrationTest, FieldAssignmentOnNonInstance_ReportsRuntimeError) {
    std::ostringstream out;
    run("var x = \"hello\";\nx.field = 1;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> >>> 인스턴스가 아닌 대상에 필드를 대입했습니다.\n>>> ");
}

// 참고: This를 클래스 메서드 밖에서 사용하는 것은 Checker::checkThis가 정적으로
// 막는다 (CheckerTest.cpp의 ThisOutsideClassReportsError 참고) - Executor까지
// 도달하지 않으므로 CheckerError의 "[N번째 줄]" 접두사와 메시지가 그대로 노출된다.
TEST_F(RunPromptShellIntegrationTest, ThisOutsideMethod_ReportsCheckError) {
    std::ostringstream out;
    run("print This;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> [1번째 줄] 클래스 메서드 밖에서 This를 사용할 수 없습니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, InstanceOf_SameClass_PrintsTrue) {
    std::ostringstream out;
    run("Class Robot { }\nvar r = Robot();\nprint r instanceof Robot;\n", out);

    EXPECT_EQ(programOutput.str(), "true\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, InstanceOf_DifferentClass_PrintsFalse) {
    std::ostringstream out;
    run("Class Robot { }\nClass Cat { }\nvar r = Robot();\nprint r instanceof Cat;\n", out);

    EXPECT_EQ(programOutput.str(), "false\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, InstanceOf_NonInstanceOperand_PrintsFalse) {
    std::ostringstream out;
    run("Class Robot { }\nprint 1 instanceof Robot;\n", out);

    EXPECT_EQ(programOutput.str(), "false\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> ");
}

// --- 8. Library import ---
// AssemblerImportTest.cpp/ExecutorImportTest.cpp는 각각 단일 Unit만 검증하므로,
// 여기서는 실제 파일 시스템(FileSourceReader)까지 포함한 4-Unit 전체 파이프라인으로
// 슬라이드의 import 예시/오류 케이스를 재현한다.

TEST_F(RunPromptShellIntegrationTest, Import_LibraryFunctionAccessibleThroughAlias_PrintsSum) {
    std::string path = writeTempFile("codefab_import_sum.cf", "Func add(a, b) {\n    return a + b;\n}\n");

    std::ostringstream out;
    run("import \"" + path + "\" alias sum;\nprint sum.add(1, 2);\n", out);

    EXPECT_EQ(programOutput.str(), "3\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, Import_FileNotFound_ReportsAssemblerError) {
    std::ostringstream out;
    run("import \"이런_파일은_없습니다.cf\" alias x;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "),
                                  HasSubstr("import 대상 파일을 열 수 없습니다"),
                                  EndsWith(">>> ")));
}

// 세부 규칙: import문은 반복문(for) 안에서는 사용할 수 없다.
TEST_F(RunPromptShellIntegrationTest, Import_InsideForLoop_ReportsCheckError) {
    std::string path = writeTempFile("codefab_import_loop.cf", "Func add(a, b) {\n    return a + b;\n}\n");

    std::ostringstream out;
    run("for (var i = 0; i < 1; i = i + 1) { import \"" + path + "\" alias sum; }\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> "),
                                  HasSubstr("반복문(for) 안에서는 import를 사용할 수 없습니다."),
                                  EndsWith(">>> ")));
}

// 세부 규칙: 같은 scope에서는 같은 alias로 import를 두 번 할 수 없다.
TEST_F(RunPromptShellIntegrationTest, Import_DuplicateAliasInSameScope_ReportsCheckError) {
    std::string path = writeTempFile("codefab_import_dup.cf", "Func add(a, b) {\n    return a + b;\n}\n");

    std::ostringstream out;
    run("import \"" + path + "\" alias sum;\nimport \"" + path + "\" alias sum;\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> >>> "),
                                  HasSubstr("'sum'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다."),
                                  EndsWith(">>> ")));
}

// TODO.md #13 잔여 갭: 지금까지는 함수 접근(sum.add(...))만 검증했다 - var로 export된
// 변수 접근(math.pi)도 동일하게 동작하는지 확인한다.
TEST_F(RunPromptShellIntegrationTest, Import_LibraryVariableAccessibleThroughAlias_PrintsPi) {
    std::string path = writeTempFile("codefab_import_pi.cf", "var pi = 3;\n");

    std::ostringstream out;
    run("import \"" + path + "\" alias math;\nprint math.pi;\n", out);

    EXPECT_EQ(programOutput.str(), "3\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> ");
}

// TODO.md #13 잔여 갭: 서로 다른 블록 스코프에서 같은 파일을 같은 alias로 각각
// import해도 서로 간섭하지 않아야 하고(각 블록 안에서 독립적으로 정상 동작),
// 블록을 벗어나면 그 alias는 더 이상 보이지 않아야 한다(선언되지 않은 변수 오류).
TEST_F(RunPromptShellIntegrationTest, Import_SameModuleInSeparateBlockScopes_DoesNotInterfereAndAliasDoesNotLeak) {
    std::string path = writeTempFile("codefab_import_scoped.cf", "Func add(a, b) {\n    return a + b;\n}\n");

    std::ostringstream out;
    run("{ import \"" + path + "\" alias sum; print sum.add(1, 2); }\n"
        "{ import \"" + path + "\" alias sum; print sum.add(3, 4); }\n"
        "print sum.add(1, 1);\n", out);

    EXPECT_EQ(programOutput.str(), "3\n7\n");
    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> >>> >>> "),
                                  HasSubstr("'sum'에러: 선언되지 않은 변수입니다."),
                                  EndsWith(">>> ")));
}

// TODO.md #13 잔여 갭: if절 내부에서 import가 실행되면(조건이 true이므로) 그 블록
// 안에서는 정상 동작하고, 블록을 벗어나면 다른 블록 스코프와 마찬가지로 alias가
// 보이지 않아야 한다.
TEST_F(RunPromptShellIntegrationTest, Import_InsideIfBlock_AliasOnlyExistsWithinThatBlock) {
    std::string path = writeTempFile("codefab_import_if.cf", "Func add(a, b) {\n    return a + b;\n}\n");

    std::ostringstream out;
    run("if (true) { import \"" + path + "\" alias sum; print sum.add(1, 2); }\n"
        "print sum.add(1, 1);\n", out);

    EXPECT_EQ(programOutput.str(), "3\n");
    EXPECT_THAT(out.str(), AllOf(StartsWith(">>> >>> "),
                                  HasSubstr("'sum'에러: 선언되지 않은 변수입니다."),
                                  EndsWith(">>> ")));
}

// TODO.md #13 잔여 갭: 2개 이상의 module을 동시에 import했을 때, 서로 다른
// module에 동일한 이름(var value)이 있어도 각각 다른 alias로 접근하면 값이
// 서로 섞이지 않아야 한다.
TEST_F(RunPromptShellIntegrationTest, Import_TwoModulesWithSameMemberName_CrossAliasAccessDoesNotCollide) {
    std::string pathA = writeTempFile("codefab_import_a.cf", "var value = 1;\n");
    std::string pathB = writeTempFile("codefab_import_b.cf", "var value = 2;\n");

    std::ostringstream out;
    run("import \"" + pathA + "\" alias a;\n"
        "import \"" + pathB + "\" alias b;\n"
        "print a.value;\n"
        "print b.value;\n", out);

    EXPECT_EQ(programOutput.str(), "1\n2\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> >>> ");
}

// --- 9. 상속 (Inheritance) ---
// Checker::checkClass/checkSuper, Executor::findMethod/resolveSuperclass가
// 구현되어 있어 더 이상 DISABLED_가 아니다.

TEST_F(RunPromptShellIntegrationTest, SuperCallInvokesParentMethod_PrintsBothMessages) {
    std::ostringstream out;
    run("Class Robot { move(dist) { print \"move\"; } }\n"
        "Class SpeedRobot : Robot { move(dist) { Super.move(dist); print \"Speeeed!\"; } }\n"
        "SpeedRobot().move(3);\n", out);

    EXPECT_EQ(programOutput.str(), "move\nSpeeeed!\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, MethodOverriding_ChildMethodTakesPrecedence) {
    std::ostringstream out;
    run("Class Robot { move(dist) { print \"robot move\"; } }\n"
        "Class SpeedRobot : Robot { move(dist) { print \"speed move\"; } }\n"
        "SpeedRobot().move(3);\n", out);

    EXPECT_EQ(programOutput.str(), "speed move\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, InstanceOf_ParentClass_PrintsTrueForChildInstance) {
    std::ostringstream out;
    run("Class Robot { init(name) { This.name = name; } }\n"
        "Class SpeedRobot : Robot { init(name) { Super.init(name); } }\n"
        "var w = SpeedRobot(\"Sam\");\n"
        "print (w instanceof SpeedRobot);\n"
        "print (w instanceof Robot);\n", out);

    // 자기 자신 클래스뿐 아니라 부모 클래스를 대상으로 한 instanceof도 true여야 한다.
    EXPECT_EQ(programOutput.str(), "true\ntrue\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, SelfInheritance_ReportsCheckError) {
    std::ostringstream out;
    run("Class Robot : Robot { }\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> [1번째 줄] 'Robot' 클래스는 자기 자신을 상속할 수 없습니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, InheritingNonClassTarget_ReportsCheckError) {
    std::ostringstream out;
    run("var x = 10;\nClass Robot : x { }\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> >>> [1번째 줄] 'x'은(는) 클래스가 아니므로 상속할 수 없습니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, SuperOutsideClassMethod_ReportsCheckError) {
    std::ostringstream out;
    run("Super.move();\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> [1번째 줄] 클래스 메서드 밖에서 Super를 사용할 수 없습니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, SuperInClassWithoutSuperclass_ReportsCheckError) {
    std::ostringstream out;
    run("Class Robot { move() { Super.move(); } }\n", out);

    EXPECT_EQ(programOutput.str(), "");
    EXPECT_EQ(out.str(), ">>> [1번째 줄] 부모 클래스가 없는 클래스에서 Super를 사용할 수 없습니다.\n>>> ");
}

TEST_F(RunPromptShellIntegrationTest, ParameterReassignment_DoesNotAffectCaller) {
    std::ostringstream out;
    run("Func set(a) { a = 99; }\nvar x = 1;\nset(x);\nprint x;\n", out);

    EXPECT_EQ(programOutput.str(), "1\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> >>> ");
}

TEST_F(RunPromptShellIntegrationTest, VariableIndex_ReadsCorrectElement) {
    std::ostringstream out;
    run("var arr = Array(3);\narr[0] = 10;\narr[1] = 20;\nvar i = 1;\nprint arr[i];\n", out);

    EXPECT_EQ(programOutput.str(), "20\n");
    EXPECT_EQ(out.str(), ">>> >>> >>> >>> >>> >>> ");
}
