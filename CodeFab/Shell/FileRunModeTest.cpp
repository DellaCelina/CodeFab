#include "FileRunMode.h"

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

using ::testing::_;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Throw;

namespace {

// RunPromptShellTest.cpp와 동일한 Mock 정의. 두 테스트 파일이 같은 4개
// *Interface를 Mock으로 대체하는 패턴을 공유하지만, 헤더 하나로 묶기보다는
// 각 Shell 모드 테스트 파일이 독립적으로 자기 완결적이도록 그대로 중복해서 둔다.
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
    MOCK_METHOD(void, check, (SyntaxTree & tree), (override));
};

class MockExecutor : public ExecuteInterface {
public:
    MOCK_METHOD(void, execute, (SyntaxTree & tree), (override));
    MOCK_METHOD(Value, evaluate, (Expression * expr), (override));
    MOCK_METHOD(const Environment&, environment, (), (const, override));
};

class FileRunModeTest : public ::testing::Test {
protected:
    NiceMock<MockTokenizer> tokenizer;
    NiceMock<MockAssembler> assembler;
    NiceMock<MockChecker> checker;
    NiceMock<MockExecutor> executor;

    FileRunMode mode{ tokenizer, assembler, checker, executor };

    // 파이프라인 호출 여부를 검증하는 테스트들은 "파일을 열 수 있다"는 사실만
    // 필요하고 실제 내용은 Tokenizer가 Mock이라 의미가 없다. 그래도 매 테스트가
    // 서로 다른 파일을 건드리지 않도록 테스트 이름 기반의 임시 경로를 쓴다.
    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "FileRunModeTest_temp.fab";

    void SetUp() override {
        std::ofstream file(tempPath);
        file << "var a = 3;\n";
    }

    void TearDown() override {
        std::filesystem::remove(tempPath);
    }
};

}  // namespace

TEST_F(FileRunModeTest, FileNotFound_ReportsErrorWithoutTokenizingAndReturnsFalse) {
    EXPECT_CALL(tokenizer, tokenize(_)).Times(0);

    std::ostringstream out;
    bool result = mode.run("이런_파일은_없습니다.fab", out);

    EXPECT_FALSE(result);
    EXPECT_THAT(out.str(), HasSubstr("이런_파일은_없습니다.fab"));
}

TEST_F(FileRunModeTest, PathIsDirectory_ReportsErrorWithoutTokenizingAndReturnsFalse) {
    // filePath는 항상 파일 1개(단일 파일)여야 한다. 디렉터리가 오면 파이프라인을
    // 시작하지 않고 명확한 오류로 거부해야 한다.
    EXPECT_CALL(tokenizer, tokenize(_)).Times(0);

    std::ostringstream out;
    bool result = mode.run(std::filesystem::temp_directory_path().string(), out);

    EXPECT_FALSE(result);
    EXPECT_THAT(out.str(), HasSubstr("파일 1개"));
}

TEST_F(FileRunModeTest, ValidFile_CallsPipelineInOrderAndReturnsTrue) {
    InSequence seq;
    EXPECT_CALL(tokenizer, tokenize(_)).WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_));
    EXPECT_CALL(executor, execute(_));

    std::ostringstream out;
    bool result = mode.run(tempPath.string(), out);

    EXPECT_TRUE(result);
    EXPECT_EQ(out.str(), "");
}

TEST_F(FileRunModeTest, TokenizerThrows_ReportsMessageAndReturnsFalse) {
    EXPECT_CALL(tokenizer, tokenize(_))
        .WillOnce(Throw(AssemblyError("[1번째 줄] 인식할 수 없는 문자입니다.")));
    EXPECT_CALL(assembler, assemble(_)).Times(0);
    EXPECT_CALL(checker, check(_)).Times(0);
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    bool result = mode.run(tempPath.string(), out);

    EXPECT_FALSE(result);
    EXPECT_EQ(out.str(), "[1번째 줄] 인식할 수 없는 문자입니다.\n");
}

TEST_F(FileRunModeTest, AssemblerThrows_ReportsMessageAndReturnsFalse) {
    EXPECT_CALL(tokenizer, tokenize(_)).WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_))
        .WillOnce(Throw(AssemblerError("'+' 다음에 피연산자가 필요합니다.")));
    EXPECT_CALL(checker, check(_)).Times(0);
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    bool result = mode.run(tempPath.string(), out);

    EXPECT_FALSE(result);
    EXPECT_EQ(out.str(), "'+' 다음에 피연산자가 필요합니다.\n");
}

TEST_F(FileRunModeTest, CheckerThrows_ReportsMessageAndReturnsFalse) {
    EXPECT_CALL(tokenizer, tokenize(_)).WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_))
        .WillOnce(Throw(CheckerError("[2번째 줄] 'a'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.")));
    EXPECT_CALL(executor, execute(_)).Times(0);

    std::ostringstream out;
    bool result = mode.run(tempPath.string(), out);

    EXPECT_FALSE(result);
    EXPECT_EQ(out.str(), "[2번째 줄] 'a'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.\n");
}

TEST_F(FileRunModeTest, ExecutorThrows_ReportsMessageAndReturnsFalse) {
    EXPECT_CALL(tokenizer, tokenize(_)).WillOnce(Return(std::vector<Token>{}));
    EXPECT_CALL(assembler, assemble(_)).WillOnce(Return(ByMove(SyntaxTree())));
    EXPECT_CALL(checker, check(_));
    EXPECT_CALL(executor, execute(_)).WillOnce(Throw(ExecutorError("0으로 나눈 오류")));

    std::ostringstream out;
    bool result = mode.run(tempPath.string(), out);

    EXPECT_FALSE(result);
    EXPECT_EQ(out.str(), "0으로 나눈 오류\n");
}

// ============================================================================
// Integration Test — Tokenizer/Assembler/Checker/Executor 전부 실제 구현을
// 사용한다. RunPromptShellTest.cpp의 RunPromptShellIntegrationTest와 동일한
// 생성 순서 원칙(참조로 물고 있는 대상이 먼저 생성되어야 함)을 따른다.
// ============================================================================
namespace {

class FileRunModeIntegrationTest : public ::testing::Test {
protected:
    Tokenizer tokenizer;
    FileSourceReader sourceReader{ tokenizer };
    Assembler assembler{ sourceReader };
    std::ostringstream programOutput;  // Executor가 print 결과를 쓰는 곳 (out과는 별개)
    Executor executor{ programOutput };
    Checker checker;

    FileRunMode mode{ tokenizer, assembler, checker, executor };

    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "FileRunModeIntegrationTest_temp.fab";

    void writeFile(const std::string& content) {
        std::ofstream file(tempPath);
        file << content;
    }

    void TearDown() override {
        std::filesystem::remove(tempPath);
    }
};

}  // namespace

TEST_F(FileRunModeIntegrationTest, ValidScript_PrintsResultAndReturnsTrue) {
    writeFile("print 1 + 2 * 3;\n");

    std::ostringstream out;
    bool result = mode.run(tempPath.string(), out);

    EXPECT_TRUE(result);
    EXPECT_EQ(programOutput.str(), "7\n");
    EXPECT_EQ(out.str(), "");
}

TEST_F(FileRunModeIntegrationTest, MultiStatementScript_ExecutesAllStatements) {
    writeFile("var a = 10;\nvar b = 20;\nprint a + b;\n");

    std::ostringstream out;
    bool result = mode.run(tempPath.string(), out);

    EXPECT_TRUE(result);
    EXPECT_EQ(programOutput.str(), "30\n");
    EXPECT_EQ(out.str(), "");
}

TEST_F(FileRunModeIntegrationTest, MissingSemicolon_ReportsSyntaxErrorAndReturnsFalse) {
    writeFile("print 1 + 2\n");

    std::ostringstream out;
    bool result = mode.run(tempPath.string(), out);

    EXPECT_FALSE(result);
    EXPECT_EQ(programOutput.str(), "");
    EXPECT_THAT(out.str(), HasSubstr("Expect ';' after value."));
}

TEST_F(FileRunModeIntegrationTest, UnclosedBraceAtEndOfFile_ReportsErrorInsteadOfWaiting) {
    writeFile("if (true) {\nprint 1;\n");

    std::ostringstream out;
    bool result = mode.run(tempPath.string(), out);

    EXPECT_FALSE(result);
    EXPECT_EQ(programOutput.str(), "");
    EXPECT_NE(out.str(), "");
}

TEST_F(FileRunModeIntegrationTest, FileDoesNotExist_ReportsErrorAndReturnsFalse) {
    std::filesystem::path missing =
        std::filesystem::temp_directory_path() / "FileRunModeIntegrationTest_missing.fab";

    std::ostringstream out;
    bool result = mode.run(missing.string(), out);

    EXPECT_FALSE(result);
    EXPECT_EQ(programOutput.str(), "");
    EXPECT_THAT(out.str(), HasSubstr(missing.string()));
}

TEST_F(FileRunModeIntegrationTest, PathIsDirectory_ReportsErrorAndReturnsFalse) {
    // 파일 1개(단일 파일)가 아닌 디렉터리 경로를 넘기면, 실제 파이프라인까지
    // 가지 않고 명확한 오류로 거부해야 한다.
    std::ostringstream out;
    bool result = mode.run(std::filesystem::temp_directory_path().string(), out);

    EXPECT_FALSE(result);
    EXPECT_EQ(programOutput.str(), "");
    EXPECT_THAT(out.str(), HasSubstr("파일 1개"));
}
