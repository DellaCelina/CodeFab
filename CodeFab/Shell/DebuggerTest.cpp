#include "Debugger.h"

#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "../Assembler/Assembler.h"
#include "../Assembler/FileSourceReader.h"
#include "../Assembler/SyntaxTree.h"
#include "../Executor/Executor.h"
#include "../Tokenizer/Tokenizer.h"

// Debugger는 ExecuteInterface::environment()/Statement::getLine()/
// containsLine() 등 전부 실제로 동작하는 API에만 의존하므로(Debugger.h 상단
// 주석 참고), 이 테스트는 Mock 없이 실제 Tokenizer/Assembler/Executor로 만든
// 진짜 SyntaxTree/Statement와 진짜 Environment를 그대로 사용한다.
class DebuggerTest : public ::testing::Test {
protected:
    Tokenizer tokenizer;
    FileSourceReader sourceReader{ tokenizer };
    Assembler assembler{ sourceReader };
    std::ostringstream programOutput;
    Executor executor{ programOutput };

    std::istringstream commandInput;
    std::ostringstream debugOutput;

    // source를 "{ }"로 감싸 실제로 파싱하고 실행한다(블록이므로 실행이
    // 끝나면 popScope로 그 안에서 선언한 변수는 사라진다). 반환된
    // BlockStatement::statements를 통해 각 문장에 대응하는 실제
    // Statement*(와 실제 줄 번호)를 얻을 수 있다 - watch/breakpoint 테스트가
    // onStatement()에 넘길 대상으로 쓴다.
    SyntaxTree parseAndExecute(const std::string& source) {
        std::vector<Token> tokens = tokenizer.tokenize("{" + source + "}");
        SyntaxTree tree = assembler.assemble(tokens);
        executor.execute(tree);
        return tree;
    }

    // source(예: "var a = 3;")를 블록으로 감싸지 않고 그대로 최상위
    // statement로 실행한다 - Environment는 스코프를 하나(전역) 갖고 시작하고
    // 그 전역 스코프는 절대 pop되지 않으므로, 이렇게 선언한 변수는 이후
    // parseAndExecute()가 만드는 다른 블록들이 끝나도 계속 살아있다. watch/
    // inspect 테스트가 "여러 번의 정지에 걸쳐 값이 유지되는 변수"를 준비할 때
    // 쓴다.
    void declareGlobal(const std::string& source) {
        std::vector<Token> tokens = tokenizer.tokenize(source);
        SyntaxTree tree = assembler.assemble(tokens);
        executor.execute(tree);
    }

    void setCommands(const std::string& commands) {
        commandInput = std::istringstream(commands);
    }
};

TEST_F(DebuggerTest, StepMode_StopsOnFirstStatementAndPrintsLine) {
    SyntaxTree tree = parseAndExecute("print 1;\nprint 2;\n");
    auto* block = dynamic_cast<BlockStatement*>(tree.getRoot());
    ASSERT_EQ(block->statements.size(), 2u);

    setCommands("continue\n");
    Debugger debugger(executor, commandInput, debugOutput);

    debugger.onStatement(block->statements[0], /*depth=*/1);

    EXPECT_EQ(debugOutput.str(), "[DEBUG] 1번째 줄에서 정지\n(debug) ");
}

TEST_F(DebuggerTest, ContinueCommand_SwitchesToContinueModeAndStopsOnlyAtBreakpoints) {
    SyntaxTree tree = parseAndExecute("print 1;\nprint 2;\nprint 3;\n");
    auto* block = dynamic_cast<BlockStatement*>(tree.getRoot());
    ASSERT_EQ(block->statements.size(), 3u);

    // 1번째 줄에서 멈춰 "break 3" -> "continue"를 입력한다.
    setCommands("break 3\ncontinue\n");
    Debugger debugger(executor, commandInput, debugOutput);
    debugger.onStatement(block->statements[0], 1);  // line 1, 멈춤

    debugOutput.str("");  // 이후 검증을 단순화하기 위해 지금까지 출력을 비운다.

    debugger.onStatement(block->statements[1], 1);  // line 2, breakpoint 아님 -> 안 멈춤
    EXPECT_EQ(debugOutput.str(), "");

    debugger.onStatement(block->statements[2], 1);  // line 3, breakpoint -> 멈춤
    EXPECT_EQ(debugOutput.str(), "[DEBUG] 3번째 줄에서 정지\n(debug) ");
}

TEST_F(DebuggerTest, NextCommand_SkipsDeeperDepthAndStopsAtSameOrShallowerDepth) {
    SyntaxTree tree = parseAndExecute("print 1;\n");
    auto* block = dynamic_cast<BlockStatement*>(tree.getRoot());
    Statement* stmt = block->statements[0];

    setCommands("next\n");
    Debugger debugger(executor, commandInput, debugOutput);

    debugger.onStatement(stmt, /*depth=*/1);  // 멈춰서 "next" 입력 -> nextTargetDepth_ = 1
    debugOutput.str("");

    debugger.onStatement(stmt, /*depth=*/2);  // 더 깊은 depth(하위 statement) -> 건너뜀
    EXPECT_EQ(debugOutput.str(), "");

    debugger.onStatement(stmt, /*depth=*/1);  // 같은 depth로 복귀 -> 멈춤
    EXPECT_EQ(debugOutput.str(), "[DEBUG] 1번째 줄에서 정지\n(debug) ");
}

TEST_F(DebuggerTest, WatchCommand_PrintsValueOnNextStopButNotImmediately) {
    // var a는 블록으로 감싸지 않고 전역 스코프에 직접 선언해, 아래 두 번째
    // parseAndExecute()가 만드는 블록(과 그 popScope)이 끝나도 계속 살아있게 한다.
    declareGlobal("var a = 3;\n");
    SyntaxTree tree = parseAndExecute("print a;\n");
    Statement* stmt = dynamic_cast<BlockStatement*>(tree.getRoot())->statements[0];

    // 1번째 정지: "watch a" 등록 후 "step"으로 즉시 한 번 더 멈추게 한다.
    // 두 명령 모두 promptAndHandleCommand의 루프 반복이라 "(debug) "가
    // 명령 하나당 한 번씩(총 2번) 출력된다.
    setCommands("watch a\nstep\ncontinue\n");
    Debugger debugger(executor, commandInput, debugOutput);
    debugger.onStatement(stmt, 1);

    // watch를 등록한 이 정지 시점에는 아직 watch 목록이 비어 있었으므로
    // [WATCH] 줄이 없어야 한다.
    EXPECT_EQ(debugOutput.str(), "[DEBUG] 1번째 줄에서 정지\n(debug) (debug) ");
    debugOutput.str("");

    debugger.onStatement(stmt, 1);  // step 모드라 다시 멈춘다.
    EXPECT_EQ(debugOutput.str(), "[DEBUG] 1번째 줄에서 정지\n[WATCH] a = 3\n(debug) ");
}

TEST_F(DebuggerTest, UnwatchCommand_RemovesVariableFromWatchList) {
    declareGlobal("var a = 3;\n");
    SyntaxTree tree = parseAndExecute("print a;\n");
    Statement* stmt = dynamic_cast<BlockStatement*>(tree.getRoot())->statements[0];

    setCommands("watch a\nstep\n");
    Debugger debugger(executor, commandInput, debugOutput);
    debugger.onStatement(stmt, 1);
    debugOutput.str("");

    // 이 정지에서 printWatches()는 "unwatch a"를 처리하기 전에 이미 실행되므로
    // (watches_에 여전히 "a"가 남아있는 상태) [WATCH] a = 3이 한 번 더 찍힌다.
    // 그 다음 "unwatch a"로 제거하고 "step"으로 세 번째 정지를 유도한다.
    setCommands("unwatch a\nstep\n");
    debugger.onStatement(stmt, 1);
    EXPECT_EQ(debugOutput.str(), "[DEBUG] 1번째 줄에서 정지\n[WATCH] a = 3\n(debug) (debug) ");
    debugOutput.str("");

    // 세 번째 정지: 이제 watches_가 비어 있으므로 [WATCH] 줄이 없어야 한다 -
    // 이게 실제로 "제거됐다"는 것을 보여주는 검증이다.
    setCommands("continue\n");
    debugger.onStatement(stmt, 1);
    EXPECT_EQ(debugOutput.str(), "[DEBUG] 1번째 줄에서 정지\n(debug) ");
}

TEST_F(DebuggerTest, WatchesCommand_ListsCurrentlyWatchedNames) {
    SyntaxTree tree = parseAndExecute("print 1;\n");
    Statement* stmt = dynamic_cast<BlockStatement*>(tree.getRoot())->statements[0];

    setCommands("watch a\nwatch b\nwatches\ncontinue\n");
    Debugger debugger(executor, commandInput, debugOutput);
    debugger.onStatement(stmt, 1);

    // promptAndHandleCommand는 명령을 하나 읽을 때마다 "(debug) "를 다시
    // 출력한다(정보성 명령이라 모드를 바꾸지 않고 계속 같은 정지에서 다음
    // 명령을 기다리기 때문) - 그래서 "watch a"/"watch b"/"watches" 세 개를
    // 처리하는 동안 "(debug) "가 3번 나온 뒤에야 watches 명령의 결과가 찍힌다.
    EXPECT_EQ(debugOutput.str(),
              "[DEBUG] 1번째 줄에서 정지\n"
              "(debug) (debug) (debug) "
              "[WATCH] a = undefined\n"
              "[WATCH] b = undefined\n"
              "(debug) ");
}

TEST_F(DebuggerTest, BreakpointsCommand_ListsAndRemoveDeletesBreakpoint) {
    SyntaxTree tree = parseAndExecute("print 1;\n");
    Statement* stmt = dynamic_cast<BlockStatement*>(tree.getRoot())->statements[0];

    setCommands("break 5\nbreak 10\nbreakpoints\nremove 5\nbreakpoints\ncontinue\n");
    Debugger debugger(executor, commandInput, debugOutput);
    debugger.onStatement(stmt, 1);

    // 정보성 명령마다("break"/"remove"/"breakpoints") promptAndHandleCommand가
    // 그 결과를 출력한 "다음" 반복에서 새로 "(debug) "를 찍고서야 그 다음
    // 명령을 읽으므로, 각 결과 줄 앞에 "(debug) "가 하나씩 붙는다.
    EXPECT_EQ(debugOutput.str(),
              "[DEBUG] 1번째 줄에서 정지\n"
              "(debug) [BREAK] 5번째 줄에 브레이크포인트를 설정했습니다.\n"
              "(debug) [BREAK] 10번째 줄에 브레이크포인트를 설정했습니다.\n"
              "(debug) [BREAK] 5, 10\n"
              "(debug) [BREAK] 5번째 줄의 브레이크포인트를 제거했습니다.\n"
              "(debug) [BREAK] 10\n"
              "(debug) ");
}

TEST_F(DebuggerTest, InspectCommand_ListsAllDefinedVariablesWithValues) {
    declareGlobal("var a = 3;\n");
    declareGlobal("var b = \"hi\";\n");
    SyntaxTree tree = parseAndExecute("print a;\n");
    Statement* stmt = dynamic_cast<BlockStatement*>(tree.getRoot())->statements[0];

    setCommands("inspect\ncontinue\n");
    Debugger debugger(executor, commandInput, debugOutput);
    debugger.onStatement(stmt, 1);

    std::string out = debugOutput.str();
    EXPECT_NE(out.find("[INSPECT] a = 3"), std::string::npos);
    EXPECT_NE(out.find("[INSPECT] b = hi"), std::string::npos);
}

TEST_F(DebuggerTest, UnknownCommand_PrintsErrorAndKeepsPrompting) {
    SyntaxTree tree = parseAndExecute("print 1;\n");
    Statement* stmt = dynamic_cast<BlockStatement*>(tree.getRoot())->statements[0];

    setCommands("frobnicate\ncontinue\n");
    Debugger debugger(executor, commandInput, debugOutput);
    debugger.onStatement(stmt, 1);

    EXPECT_EQ(debugOutput.str(),
              "[DEBUG] 1번째 줄에서 정지\n"
              "(debug) [DEBUG] 알 수 없는 명령입니다: 'frobnicate'\n"
              "(debug) ");
}

TEST_F(DebuggerTest, NoMoreInput_DefaultsToContinueInsteadOfBlocking) {
    // 입력 스트림이 끝나면(EOF) 무한정 프롬프트를 다시 띄우지 않고 continue로
    // 처리해서 나머지 실행을 그냥 계속하게 한다.
    SyntaxTree tree = parseAndExecute("print 1;\nprint 2;\n");
    auto* block = dynamic_cast<BlockStatement*>(tree.getRoot());

    setCommands("");  // 즉시 EOF
    Debugger debugger(executor, commandInput, debugOutput);
    debugger.onStatement(block->statements[0], 1);
    debugOutput.str("");

    // continue 모드가 됐고 breakpoint가 없으므로 더 이상 멈추지 않는다.
    debugger.onStatement(block->statements[1], 1);
    EXPECT_EQ(debugOutput.str(), "");
}
