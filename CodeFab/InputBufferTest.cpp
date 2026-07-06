#include "InputBuffer.h"

#include "gtest/gtest.h"

TEST(InputBufferTest, EmptySource_IsComplete) {
    EXPECT_TRUE(IsInputComplete(""));
}

TEST(InputBufferTest, SimpleStatementWithoutBrackets_IsComplete) {
    EXPECT_TRUE(IsInputComplete("var a = 3;"));
}

TEST(InputBufferTest, BalancedParens_IsComplete) {
    EXPECT_TRUE(IsInputComplete("print(1 + 2);"));
}

TEST(InputBufferTest, UnbalancedOpenBrace_IsNotComplete) {
    EXPECT_FALSE(IsInputComplete("if (a > 0) {"));
}

TEST(InputBufferTest, BraceClosedOnLaterLine_IsComplete) {
    EXPECT_TRUE(IsInputComplete("if (a > 0) {\n}"));
}

TEST(InputBufferTest, NestedBraces_IsComplete) {
    EXPECT_TRUE(IsInputComplete("{ { } }"));
}

TEST(InputBufferTest, NestedBraces_StillOpen_IsNotComplete) {
    EXPECT_FALSE(IsInputComplete("{ { }"));
}

TEST(InputBufferTest, BraceInsideStringLiteral_IsNotCounted) {
    EXPECT_TRUE(IsInputComplete("print \"{\";"));
}

TEST(InputBufferTest, ParenInsideStringLiteral_IsNotCounted) {
    EXPECT_TRUE(IsInputComplete("print \"(\";"));
}

TEST(InputBufferTest, UnterminatedStringLiteral_IsNotComplete) {
    EXPECT_FALSE(IsInputComplete("print \"hello"));
}

TEST(InputBufferTest, EscapedQuoteInsideString_DoesNotCloseString) {
    // "he said \"hi\"" 전체가 하나의 문자열 리터럴로 처리되어야 하고,
    // 그 안의 이스케이프된 따옴표 때문에 조기 종료되면 안 된다.
    EXPECT_TRUE(IsInputComplete("print \"he said \\\"hi\\\"\";"));
}

TEST(InputBufferTest, StrayClosingBracket_DoesNotUnderflow) {
    // 여는 괄호 없이 등장한 닫는 괄호는 "더 입력이 필요한 상태"로 취급하지 않는다.
    // (문법 오류 여부 판정은 Assembler Unit의 책임)
    EXPECT_TRUE(IsInputComplete("}"));
}

TEST(InputBufferTest, MixedBracketTypes_AllClosed_IsComplete) {
    EXPECT_TRUE(IsInputComplete("for (var i = 0; i < arr[0]; i = i + 1) { print i; }"));
}
