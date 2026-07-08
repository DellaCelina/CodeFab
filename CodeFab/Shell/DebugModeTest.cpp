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
    EXPECT_THAT(out.str(), HasSubstr("파일 1개"));
}

TEST_F(DebugModeTest, TokenizerThrows_ReportsMessageAndReturnsFalse) {
    EXPECT_CALL(tokenizer, tokenize(_))
        .WillOnce(Throw(AssemblyError("[1번째 줄] 인식할 수 없는 문자입니다.")));
    EXPECT_CALL(assembler, assemble(_)).Times(0);
    EXPECT_CALL(checker, check(_)).Times(0);

    std::istringstream in;
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_FALSE(result);
    EXPECT_EQ(out.str(), "[1번째 줄] 인식할 수 없는 문자입니다.\n");
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

TEST_F(DebugModeTest, CheckerReturnsFalseWithoutThrowing_ReportsFailureAndReturnsFalse) {
    EXPECT_CALL(tokenizer, tokenize(_)).WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_)).WillOnce(Return(false));

    std::istringstream in;
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_FALSE(result);
    EXPECT_EQ(out.str(), "코드 검사에 실패했습니다.\n");
}

TEST_F(DebugModeTest, CheckerThrows_ReportsMessageAndReturnsFalse) {
    EXPECT_CALL(tokenizer, tokenize(_)).WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_))
        .WillOnce(Throw(CheckerError("[2번째 줄] 'a'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.")));

    std::istringstream in;
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_FALSE(result);
    EXPECT_EQ(out.str(), "[2번째 줄] 'a'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.\n");
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
    EXPECT_EQ(out.str(), "[DEBUG] 1번째 줄에서 정지\n(debug) ");
}

TEST_F(DebugModeIntegrationTest, Breakpoint_StopsAtTargetLineThenFinishesOnContinue) {
    writeFile("print 1;\nprint 2;\nprint 3;\n");

    std::istringstream in("break 3\ncontinue\ncontinue\n");
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_TRUE(result);
    EXPECT_EQ(programOutput.str(), "1\n2\n3\n");
    EXPECT_EQ(out.str(),
              "[DEBUG] 1번째 줄에서 정지\n"
              "(debug) [BREAK] 3번째 줄에 브레이크포인트를 설정했습니다.\n"
              "(debug) [DEBUG] 3번째 줄에서 정지\n"
              "(debug) ");
}

TEST_F(DebugModeIntegrationTest, WatchAcrossSteps_ShowsUndefinedBeforeDeclarationAndValueAfter) {
    writeFile("var a = 3;\nprint a;\n");

    std::istringstream in("watch a\nstep\nstep\n");
    std::ostringstream out;
    bool result = mode.run(tempPath.string(), in, out);

    EXPECT_TRUE(result);
    EXPECT_EQ(programOutput.str(), "3\n");
    EXPECT_EQ(out.str(),
              "[DEBUG] 1번째 줄에서 정지\n"
              "(debug) (debug) "
              "[DEBUG] 1번째 줄에서 정지\n"
              "[WATCH] a = undefined\n"
              "(debug) "
              "[DEBUG] 2번째 줄에서 정지\n"
              "[WATCH] a = 3\n"
              "(debug) ");
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
