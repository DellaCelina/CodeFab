#include "CommandLineArgs.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace {

// argv는 char* 배열이어야 하므로, std::string들을 담은 벡터가 살아있는 동안만
// 유효한 char* 벡터를 만들어 반환한다. argv[0](실행 파일 경로)은 항상 채워 넣는다.
std::vector<char*> makeArgv(std::vector<std::string>& args) {
    std::vector<char*> argv;
    for (auto& arg : args) {
        argv.push_back(arg.data());
    }
    return argv;
}

}  // namespace

TEST(CommandLineArgsTest, NoArguments_DefaultsToReplMode) {
    std::vector<std::string> args{ "CodeFab.exe" };
    auto argv = makeArgv(args);

    CommandLineArgs result = CommandLineArgs::parse(static_cast<int>(argv.size()), argv.data());

    EXPECT_EQ(result.mode, ShellMode::Repl);
    EXPECT_EQ(result.path, "");
}

TEST(CommandLineArgsTest, RunWithPath_ParsesRunModeAndPath) {
    std::vector<std::string> args{ "CodeFab.exe", "run", "script.fab" };
    auto argv = makeArgv(args);

    CommandLineArgs result = CommandLineArgs::parse(static_cast<int>(argv.size()), argv.data());

    EXPECT_EQ(result.mode, ShellMode::Run);
    EXPECT_EQ(result.path, "script.fab");
}

TEST(CommandLineArgsTest, DebugWithPath_ParsesDebugModeAndPath) {
    std::vector<std::string> args{ "CodeFab.exe", "debug", "script.fab" };
    auto argv = makeArgv(args);

    CommandLineArgs result = CommandLineArgs::parse(static_cast<int>(argv.size()), argv.data());

    EXPECT_EQ(result.mode, ShellMode::Debug);
    EXPECT_EQ(result.path, "script.fab");
}

TEST(CommandLineArgsTest, RunWithoutPath_ThrowsInvalidArgument) {
    std::vector<std::string> args{ "CodeFab.exe", "run" };
    auto argv = makeArgv(args);

    EXPECT_THROW(CommandLineArgs::parse(static_cast<int>(argv.size()), argv.data()),
                 std::invalid_argument);
}

TEST(CommandLineArgsTest, DebugWithoutPath_ThrowsInvalidArgument) {
    std::vector<std::string> args{ "CodeFab.exe", "debug" };
    auto argv = makeArgv(args);

    EXPECT_THROW(CommandLineArgs::parse(static_cast<int>(argv.size()), argv.data()),
                 std::invalid_argument);
}

TEST(CommandLineArgsTest, UnknownMode_ThrowsInvalidArgument) {
    std::vector<std::string> args{ "CodeFab.exe", "fly" };
    auto argv = makeArgv(args);

    EXPECT_THROW(CommandLineArgs::parse(static_cast<int>(argv.size()), argv.data()),
                 std::invalid_argument);
}

TEST(CommandLineArgsTest, LongHelpFlag_ParsesHelpModeWithoutPath) {
    std::vector<std::string> args{ "CodeFab.exe", "--help" };
    auto argv = makeArgv(args);

    CommandLineArgs result = CommandLineArgs::parse(static_cast<int>(argv.size()), argv.data());

    EXPECT_EQ(result.mode, ShellMode::Help);
    EXPECT_EQ(result.path, "");
}

TEST(CommandLineArgsTest, ShortHelpFlag_ParsesHelpModeWithoutPath) {
    std::vector<std::string> args{ "CodeFab.exe", "-h" };
    auto argv = makeArgv(args);

    CommandLineArgs result = CommandLineArgs::parse(static_cast<int>(argv.size()), argv.data());

    EXPECT_EQ(result.mode, ShellMode::Help);
    EXPECT_EQ(result.path, "");
}

TEST(CommandLineArgsTest, UsageText_MentionsAllThreeModes) {
    std::string usage = CommandLineArgs::usageText();

    EXPECT_NE(usage.find("REPL"), std::string::npos);
    EXPECT_NE(usage.find("run"), std::string::npos);
    EXPECT_NE(usage.find("debug"), std::string::npos);
}
