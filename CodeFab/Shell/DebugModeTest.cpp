#include "DebugMode.h"

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
#include "../Executor/Executor.h"
#include "../Tokenizer/Token.h"
#include "../Tokenizer/TokenizeInterface.h"
#include "../Tokenizer/Tokenizer.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Throw;

namespace {

// RunPromptShellTest.cpp/FileRunModeTest.cpp와 동일한 Mock 정의. DebugMode는
// setStatementHook() 때문에 ExecuteInterface&가 아니라 구체 타입 Executor&를
// 받으므로(DebugMode.h 참고) Executor는 Mock으로 대체할 수 없다 - 항상 실제
// Executor를 쓴다. 그래서 Mock 기반 단위 테스트는 "Executor까지 도달하지 않는"
// 오류 경로만 다루고, 성공적으로 끝까지 실행되는 경로와 디버거 상호작용은
// 아래 DebugModeIntegrationTest(전부 실제 구현체)에서 검증한다 - Mock
// Assembler가 돌려주는 기본 생성 SyntaxTree는 root가 비어 있어 실제
// Executor::execute()에 넘기면 std::logic_error가 나기 때문이다.
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

class DebugModeTest : public ::testing::Test {
protected:
    NiceMock<MockTokenizer> tokenizer;
    NiceMock<MockAssembler> assembler;
    NiceMock<MockChecker> checker;
    std::ostringstream programOutput;
    Executor executor{ programOutput };

    DebugMode mode{ tokenizer, assembler, checker, executor };

    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "DebugModeTest_temp.fab";

    void SetUp() override {
        std::ofstream file(tempPath);
        file << "var a = 3;\n";
    }

    void TearDown() override {
        std::filesystem::remove(tempPath);
    }
};

}  // namespace

TEST_F(DebugModeTest, FileNotFound_ReportsErrorWithoutTokenizingAndReturnsFalse) {
    EXPECT_CALL(tokenizer, tokenize(_)).Times(0);

    std::istringstream in;
    std::ostringstream out;
    bool result = mode.run("이런_파일은_없습니다.fab", in, out);

    EXPECT_FALSE(result);
    EXPECT_THAT(out.str(), HasSubstr("이런_파일은_없습니다.fab"));
}

TEST_F(DebugModeTest, PathIsDirectory_ReportsErrorWithoutTokenizingAndReturnsFalse) {
    EXPECT_CALL(tokenizer, tokenize(_)).Times(0);

    std::istringstream in;
    std::ostringstream out;
    bool result = mode.run(std::filesystem::temp_directory_path().string(), in, out);

    EXPECT_FALSE(result);
    EXPECT_THAT(out.str(), HasSubstr("Error: path must be a single file:"));
}

TEST_F(DebugModeTest, TokenizerThrows_ReportsMessageAndReturnsFalse) {
    EXPECT_CALL(tokenizer, tokenize(_))
        .WillOnce(Throw(AssemblyError("[line 1] unknown character: '@'")));
    EXPECT_CALL(assembler, assemble(_)).Times(0);
    EXPECT_CALL(checker, check(_)).Times(0);

    std::istringstream in;
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_FALSE(result);
    EXPECT_EQ(out.str(), "[line 1] unknown character: '@'\n");
}

TEST_F(DebugModeTest, AssemblerThrows_ReportsMessageAndReturnsFalse) {
    EXPECT_CALL(tokenizer, tokenize(_)).WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_))
        .WillOnce(Throw(AssemblerError("'+' 다음에 피연산자가 필요합니다.")));
    EXPECT_CALL(checker, check(_)).Times(0);

    std::istringstream in;
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_FALSE(result);
    EXPECT_EQ(out.str(), "'+' 다음에 피연산자가 필요합니다.\n");
}


TEST_F(DebugModeTest, CheckerThrows_ReportsMessageAndReturnsFalse) {
    EXPECT_CALL(tokenizer, tokenize(_)).WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_))
        .WillOnce(Throw(CheckerError("[line 2] 'a' is already declared in this scope.")));

    std::istringstream in;
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_FALSE(result);
    EXPECT_EQ(out.str(), "[line 2] 'a' is already declared in this scope.\n");
}

// ============================================================================
// Integration Test — Tokenizer/Assembler/Checker/Executor 전부 실제 구현을
// 사용한다. DebugMode가 실제로 Debugger와 연결되어 step/continue/breakpoint/
// watch가 눈에 보이는 출력으로 이어지는지까지 검증한다(Mock 없음).
// ============================================================================
namespace {

class DebugModeIntegrationTest : public ::testing::Test {
protected:
    Tokenizer tokenizer;
    FileSourceReader sourceReader{ tokenizer };
    Assembler assembler{ sourceReader };
    std::ostringstream programOutput;
    Executor executor{ programOutput };
    Checker checker{ executor };

    DebugMode mode{ tokenizer, assembler, checker, executor };

    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "DebugModeIntegrationTest_temp.fab";

    void writeFile(const std::string& content) {
        std::ofstream file(tempPath);
        file << content;
    }

    void TearDown() override {
        std::filesystem::remove(tempPath);
    }
};

}  // namespace

TEST_F(DebugModeIntegrationTest, ContinueImmediately_RunsWholeScriptAndReturnsTrue) {
    writeFile("print 1 + 2;\n");

    std::istringstream in("continue\n");
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_TRUE(result);
    EXPECT_EQ(programOutput.str(), "3\n");
    EXPECT_EQ(out.str(), "[DEBUG] paused at line 1 -> print 1 + 2;\n> ");
}

TEST_F(DebugModeIntegrationTest, Breakpoint_StopsAtTargetLineThenFinishesOnContinue) {
    writeFile("print 1;\nprint 2;\nprint 3;\n");

    std::istringstream in("break 3\ncontinue\ncontinue\n");
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_TRUE(result);
    EXPECT_EQ(programOutput.str(), "1\n2\n3\n");
    EXPECT_EQ(out.str(),
              "[DEBUG] paused at line 1 -> print 1;\n"
              "> [BREAK] breakpoint set at line 3.\n"
              "> [DEBUG] paused at line 3 -> print 3;\n"
              "> ");
}

TEST_F(DebugModeIntegrationTest, WatchAcrossSteps_ShowsUndefinedBeforeDeclarationAndValueAfter) {
    writeFile("var a = 3;\nprint a;\n");

    std::istringstream in("watch a\nstep\nstep\n");
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_TRUE(result);
    EXPECT_EQ(programOutput.str(), "3\n");
    // "watch a"는 등록하는 즉시 현재 값을 보여준다(이 시점엔 아직 a가
    // 정의되기 전이라 undefined) - 이후 매 정지마다 printWatches가 자동으로
    // 최신 값을 다시 보여준다. 최상위 문장 2개(var a=3;, print a;)는 각각
    // 정확히 한 번씩만 멈춰야 한다 - 파일을 감싸는 합성 블록에서 중복으로
    // 멈추지 않는다(아래 MultiStatementWithNestedIfBlock_... 테스트가 이
    // 회귀를 더 명확하게 검증한다).
    EXPECT_EQ(out.str(),
              "[DEBUG] paused at line 1 -> var a = 3;\n"
              "> [WATCH] a = undefined\n"
              "> "
              "[DEBUG] paused at line 2 -> print a;\n"
              "[WATCH] a = 3\n"
              "> ");
}

TEST_F(DebugModeIntegrationTest, MultiStatementWithNestedIfBlock_StepsThroughEachRealStatementExactlyOnce) {
    // 회귀 테스트: DebugMode::run()이 파일 전체를 "{ }"로 감싸 만드는 합성
    // 최상위 블록 자체가 예전엔 Debugger에도 노출되어, 첫 번째 "step"이 실제
    // 문장을 진행시키지 못하고 1번째 줄에서 다시 멈추는 버그가 있었다(가짜
    // 문장 하나가 실제 문장들 앞에 끼어 있었음). 이 테스트는 최상위 선언 ->
    // if(중첩 블록: 선언+print) -> 최상위 print로 이어지는 실제 파일에서,
    // 정지 지점이 정확히 실제 문장 개수(6개)만큼만, 각 줄마다 한 번씩만
    // 발생하는지 확인한다 - 1번째 줄이 두 번 나오면 이 버그가 재발한 것이다.
    writeFile("var g = 1;\nif (true)\n{\n\tvar a = 1;\n\tprint a;\n}\nprint g;\n");

    std::istringstream in("step\nstep\nstep\nstep\nstep\nstep\n");
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_TRUE(result);
    EXPECT_EQ(programOutput.str(), "1\n1\n");
    EXPECT_EQ(out.str(),
              "[DEBUG] paused at line 1 -> var g = 1;\n"
              "> [DEBUG] paused at line 2 -> if (true)\n"
              "> [DEBUG] paused at line 3 -> {\n"  // 중첩 블록 자체 - 실제 소스에 있는 블록
              "> [DEBUG] paused at line 4 -> \tvar a = 1;\n"
              "> [DEBUG] paused at line 5 -> \tprint a;\n"
              "> [DEBUG] paused at line 7 -> print g;\n"
              "> ");
}

TEST_F(DebugModeIntegrationTest, NextOverIfStatement_SkipsNestedBlockAndStopsAtNextTopLevelStatement) {
    writeFile("var g = 1;\nif (true)\n{\n\tvar a = 1;\n\tprint a;\n}\nprint g;\n");

    std::istringstream in("step\nnext\ncontinue\n");
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_TRUE(result);
    EXPECT_EQ(programOutput.str(), "1\n1\n");
    // if문(2번째 줄)에서 "next"를 치면, 최상위 문장이 depth 1부터 시작하기
    // 때문에(합성 블록이 depth를 하나 차지하지 않음) 그 안의 중첩 블록/선언/
    // print(3~5번째 줄, depth 2~3)는 전부 건너뛰고 실제로 실행만 되며, 다음
    // 최상위 문장(7번째 줄)에서 멈춘다.
    EXPECT_EQ(out.str(),
              "[DEBUG] paused at line 1 -> var g = 1;\n"
              "> [DEBUG] paused at line 2 -> if (true)\n"
              "> [DEBUG] paused at line 7 -> print g;\n"
              "> ");
}

TEST_F(DebugModeIntegrationTest, MissingSemicolon_ReportsSyntaxErrorWithoutStartingDebugger) {
    writeFile("print 1 + 2\n");

    std::istringstream in;  // 디버거까지 도달하지 않으므로 입력이 필요 없다.
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_FALSE(result);
    EXPECT_EQ(programOutput.str(), "");
    EXPECT_THAT(out.str(), HasSubstr("Expect ';' after value."));
}
