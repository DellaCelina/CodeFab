#include <unordered_map>
#include <vector>

#include "gmock/gmock.h"
#include "Assembler.h"
#include "FileSourceReader.h"
#include "SourceReaderInterface.h"
#include "../Tokenizer/Tokenizer.h"

using namespace testing;

// import 관련 테스트에서 실제 파일 시스템 없이 "경로 -> 토큰 목록"을 흉내내기
// 위한 인메모리 Fake. 등록되지 않은 경로를 read()하면 예외를 던진다.
struct FakeSourceReader : public SourceReaderInterface {
    std::unordered_map<std::string, std::vector<Token>> files;

    std::vector<Token> read(const std::string& path) override {
        auto it = files.find(path);
        if (it == files.end())
            throw std::runtime_error("file not found: " + path);
        return it->second;
    }
};

struct AssemblerTester : public Test {
    Tokenizer tokenizer;
    FileSourceReader sourceReader{ tokenizer };
    Assembler assembler{ sourceReader };
};

TEST_F(AssemblerTester, PrintWithExpressionTest) {
    /*
    print 1 + 2 * 3;        // expect: 7
    */
    std::vector<Token> tokens = {
        { TokenType::PRINT, "print", 0},
        { TokenType::NUMBER, "1", 0},
        { TokenType::PLUS, "+", 0},
        { TokenType::NUMBER, "2", 0},
        { TokenType::STAR, "*", 0},
        { TokenType::NUMBER, "3", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);

    auto root = tree.getRoot();

    NumberExpression one({ tokens[1] }, 1);
    NumberExpression two({ tokens[3] }, 2);
    NumberExpression three({ tokens[5] }, 3);
    MultExpression mult({ tokens[4] }, &two, &three);
    AddExpression add({ tokens[2] }, &one, &mult);
    PrintStatement golden({ tokens[0], tokens[6] }, &add);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, SubtractionAssociativityTest) {
    // print 10 - 4 - 3;    // expect: 3  (left-associative)
    std::vector<Token> tokens = {
        { TokenType::PRINT, "print", 0},
        { TokenType::NUMBER, "10", 0},
        { TokenType::MINUS, "-", 0},
        { TokenType::NUMBER, "4", 0},
        { TokenType::MINUS, "-", 0},
        { TokenType::NUMBER, "3", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    NumberExpression ten({ tokens[1] }, 10);
    NumberExpression four({ tokens[3] }, 4);
    NumberExpression three({ tokens[5] }, 3);
    SubExpression subOne({ tokens[2] }, &ten, &four);
    SubExpression subTwo({ tokens[4] }, &subOne, &three);
    PrintStatement golden({ tokens[0], tokens[6] }, &subTwo);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, DivisionAssociativityTest) {
    // print 8 / 2 / 2;    // expect: 2  (left-associative)
    std::vector<Token> tokens = {
        { TokenType::PRINT, "print", 0},
        { TokenType::NUMBER, "8", 0},
        { TokenType::SLASH, "/", 0},
        { TokenType::NUMBER, "2", 0},
        { TokenType::SLASH, "/", 0},
        { TokenType::NUMBER, "2", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    NumberExpression eight({ tokens[1] }, 8);
    NumberExpression two1({ tokens[3] }, 2);
    NumberExpression two2({ tokens[5] }, 2);
    DivideExpression divOne({ tokens[2] }, &eight, &two1);
    DivideExpression divTwo({ tokens[4] }, &divOne, &two2);
    PrintStatement golden({ tokens[0], tokens[6] }, &divTwo);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, NegativeNumberTest) {
    // print -3 + 2;    // expect: -1
    std::vector<Token> tokens = {
        { TokenType::PRINT, "print", 0},
        { TokenType::MINUS, "-", 0},
        { TokenType::NUMBER, "3", 0},
        { TokenType::PLUS, "+", 0},
        { TokenType::NUMBER, "2", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    NumberExpression three({ tokens[2] }, 3);
    NegativeExpression negThree({ tokens[1] }, &three);
    NumberExpression two({ tokens[4] }, 2);
    AddExpression add({ tokens[3] }, &negThree, &two);
    PrintStatement golden({ tokens[0], tokens[5] }, &add);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, LessThanComparisonTest) {
    // print 1 < 2;    // expect: true
    std::vector<Token> tokens = {
        { TokenType::PRINT, "print", 0},
        { TokenType::NUMBER, "1", 0},
        { TokenType::LESS, "<", 0},
        { TokenType::NUMBER, "2", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    NumberExpression one({ tokens[1] }, 1);
    NumberExpression two({ tokens[3] }, 2);
    LessExpression less({ tokens[2] }, &one, &two);
    PrintStatement golden({ tokens[0], tokens[4] }, &less);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, GreaterThanComparisonTest) {
    // print 3 > 5;    // expect: false
    std::vector<Token> tokens = {
        { TokenType::PRINT, "print", 0},
        { TokenType::NUMBER, "3", 0},
        { TokenType::GREATER, ">", 0},
        { TokenType::NUMBER, "5", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    NumberExpression three({ tokens[1] }, 3);
    NumberExpression five({ tokens[3] }, 5);
    GreaterExpression greater({ tokens[2] }, &three, &five);
    PrintStatement golden({ tokens[0], tokens[4] }, &greater);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, StringConcatTest) {
    // print "Hello, " + "CodeFab!";    // expect: Hello, CodeFab!
    std::vector<Token> tokens = {
        { TokenType::PRINT, "print", 0},
        { TokenType::STRING, "Hello, ", 0},
        { TokenType::PLUS, "+", 0},
        { TokenType::STRING, "CodeFab!", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    StringExpression hello({ tokens[1] }, "Hello, ");
    StringExpression codefab({ tokens[3] }, "CodeFab!");
    AddExpression add({ tokens[2] }, &hello, &codefab);
    PrintStatement golden({ tokens[0], tokens[4] }, &add);

    EXPECT_EQ(*root, golden);
}


TEST_F(AssemblerTester, DeclareStatementTest) {
    // var a = 10;
    std::vector<Token> tokens = {
        { TokenType::VAR, "var", 0},
        { TokenType::IDENTIFIER, "a", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::NUMBER, "10", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression a({ tokens[1] }, "a");
    NumberExpression ten({ tokens[3] }, 10);
    DeclareStatement golden({ tokens[0], tokens[2], tokens[4] }, &a, &ten);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, ReassignmentTest) {
    // a = a + 5;
    std::vector<Token> tokens = {
        { TokenType::IDENTIFIER, "a", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::IDENTIFIER, "a", 0},
        { TokenType::PLUS, "+", 0},
        { TokenType::NUMBER, "5", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression target({ tokens[0] }, "a");
    IdentifierExpression a({ tokens[2] }, "a");
    NumberExpression five({ tokens[4] }, 5);
    AddExpression add({ tokens[3] }, &a, &five);
    AssignExpression assign({ tokens[1] }, &target, &add);
    ExpressionStatement golden({ tokens[5] }, &assign);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, BlockScopeTest) {
    // { var x = "inner"; print x; }
    std::vector<Token> tokens = {
        { TokenType::LEFT_BRACE, "{", 0},
        { TokenType::VAR, "var", 0},
        { TokenType::IDENTIFIER, "x", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::STRING, "inner", 0},
        { TokenType::SEMICOLON, ";", 0},
        { TokenType::PRINT, "print", 0},
        { TokenType::IDENTIFIER, "x", 0},
        { TokenType::SEMICOLON, ";", 0},
        { TokenType::RIGHT_BRACE, "}", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression x_decl({ tokens[2] }, "x");
    StringExpression inner({ tokens[4] }, "inner");
    DeclareStatement declareX({ tokens[1], tokens[3], tokens[5] }, &x_decl, &inner);
    IdentifierExpression x({ tokens[7] }, "x");
    PrintStatement printX({ tokens[6], tokens[8] }, &x);
    BlockStatement golden({ tokens[0], tokens[9] }, { &declareX, &printX });

    EXPECT_EQ(*root, golden);
}


TEST_F(AssemblerTester, IfStatementTest) {
    // if (true) print "bbq";    // expect: bbq
    std::vector<Token> tokens = {
        { TokenType::IF, "if", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::TRUE, "true", 0},
        { TokenType::RIGHT_PAREN, ")", 0},
        { TokenType::PRINT, "print", 0},
        { TokenType::STRING, "bbq", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    BooleanExpression condition({ tokens[2] }, true);
    StringExpression bbq({ tokens[5] }, "bbq");
    PrintStatement thenBranch({ tokens[4], tokens[6] }, &bbq);
    IfStatement golden({ tokens[0], tokens[1], tokens[3] }, &condition, &thenBranch);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, IfElseStatementTest) {
    // if (false) print "no"; else print "kfc";    // expect: kfc
    std::vector<Token> tokens = {
        { TokenType::IF, "if", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::FALSE, "false", 0},
        { TokenType::RIGHT_PAREN, ")", 0},
        { TokenType::PRINT, "print", 0},
        { TokenType::STRING, "no", 0},
        { TokenType::SEMICOLON, ";", 0},
        { TokenType::ELSE, "else", 0},
        { TokenType::PRINT, "print", 0},
        { TokenType::STRING, "kfc", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    BooleanExpression condition({ tokens[2] }, false);
    StringExpression no({ tokens[5] }, "no");
    PrintStatement thenBranch({ tokens[4], tokens[6] }, &no);
    StringExpression kfc({ tokens[9] }, "kfc");
    PrintStatement elseBranch({ tokens[8], tokens[10] }, &kfc);
    IfStatement golden({ tokens[0], tokens[1], tokens[3], tokens[7] }, &condition, &thenBranch, &elseBranch);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, ForStatementTest) {
    // for (j = 0; j < 3; j = j + 1) { print j; }
    std::vector<Token> tokens = {
        { TokenType::FOR, "for", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::IDENTIFIER, "j", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::NUMBER, "0", 0},
        { TokenType::SEMICOLON, ";", 0},
        { TokenType::IDENTIFIER, "j", 0},
        { TokenType::LESS, "<", 0},
        { TokenType::NUMBER, "3", 0},
        { TokenType::SEMICOLON, ";", 0},
        { TokenType::IDENTIFIER, "j", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::IDENTIFIER, "j", 0},
        { TokenType::PLUS, "+", 0},
        { TokenType::NUMBER, "1", 0},
        { TokenType::RIGHT_PAREN, ")", 0},
        { TokenType::LEFT_BRACE, "{", 0},
        { TokenType::PRINT, "print", 0},
        { TokenType::IDENTIFIER, "j", 0},
        { TokenType::SEMICOLON, ";", 0},
        { TokenType::RIGHT_BRACE, "}", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression initTarget({ tokens[2] }, "j");
    NumberExpression zero({ tokens[4] }, 0);
    AssignExpression initAssign({ tokens[3] }, &initTarget, &zero);
    ExpressionStatement init({ tokens[5] }, &initAssign);
    IdentifierExpression jCompare({ tokens[6] }, "j");
    NumberExpression three({ tokens[8] }, 3);
    LessExpression compare({ tokens[7] }, &jCompare, &three);
    IdentifierExpression nextTarget({ tokens[10] }, "j");
    IdentifierExpression jNext({ tokens[12] }, "j");
    NumberExpression one({ tokens[14] }, 1);
    AddExpression jPlusOne({ tokens[13] }, &jNext, &one);
    AssignExpression next({ tokens[11] }, &nextTarget, &jPlusOne);
    IdentifierExpression jPrint({ tokens[18] }, "j");
    PrintStatement printJ({ tokens[17], tokens[19] }, &jPrint);
    BlockStatement loop({ tokens[16], tokens[20] }, { &printJ });
    ForStatement golden({ tokens[0], tokens[1], tokens[9], tokens[15] },
        &init, &compare, &next, &loop);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, ForStatementWithVarInitializerTest) {
    // for (var j = 0; j < 3; j = j + 1) { print j; }
    std::vector<Token> tokens = {
        { TokenType::FOR, "for", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::VAR, "var", 0},
        { TokenType::IDENTIFIER, "j", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::NUMBER, "0", 0},
        { TokenType::SEMICOLON, ";", 0},
        { TokenType::IDENTIFIER, "j", 0},
        { TokenType::LESS, "<", 0},
        { TokenType::NUMBER, "3", 0},
        { TokenType::SEMICOLON, ";", 0},
        { TokenType::IDENTIFIER, "j", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::IDENTIFIER, "j", 0},
        { TokenType::PLUS, "+", 0},
        { TokenType::NUMBER, "1", 0},
        { TokenType::RIGHT_PAREN, ")", 0},
        { TokenType::LEFT_BRACE, "{", 0},
        { TokenType::PRINT, "print", 0},
        { TokenType::IDENTIFIER, "j", 0},
        { TokenType::SEMICOLON, ";", 0},
        { TokenType::RIGHT_BRACE, "}", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression initName({ tokens[3] }, "j");
    NumberExpression zero({ tokens[5] }, 0);
    DeclareStatement init({ tokens[2], tokens[4], tokens[6] }, &initName, &zero);
    IdentifierExpression jCompare({ tokens[7] }, "j");
    NumberExpression three({ tokens[9] }, 3);
    LessExpression compare({ tokens[8] }, &jCompare, &three);
    IdentifierExpression nextTarget({ tokens[11] }, "j");
    IdentifierExpression jNext({ tokens[13] }, "j");
    NumberExpression one({ tokens[15] }, 1);
    AddExpression jPlusOne({ tokens[14] }, &jNext, &one);
    AssignExpression next({ tokens[12] }, &nextTarget, &jPlusOne);
    IdentifierExpression jPrint({ tokens[19] }, "j");
    PrintStatement printJ({ tokens[18], tokens[20] }, &jPrint);
    BlockStatement loop({ tokens[17], tokens[21] }, { &printJ });
    ForStatement golden({ tokens[0], tokens[1], tokens[10], tokens[16] },
        &init, &compare, &next, &loop);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, MissingSemicolonThrowsTest) {
    // print 1 + 2   (세미콜론 누락)
    std::vector<Token> tokens = {
        { TokenType::PRINT, "print", 0},
        { TokenType::NUMBER, "1", 0},
        { TokenType::PLUS, "+", 0},
        { TokenType::NUMBER, "2", 0},
    };

    EXPECT_THROW(assembler.assemble(tokens), AssemblerError);
}

TEST_F(AssemblerTester, MissingClosingParenThrowsTest) {
    // print (1 + 2;   (닫는 괄호 누락)
    std::vector<Token> tokens = {
        { TokenType::PRINT, "print", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::NUMBER, "1", 0},
        { TokenType::PLUS, "+", 0},
        { TokenType::NUMBER, "2", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    EXPECT_THROW(assembler.assemble(tokens), AssemblerError);
}

TEST_F(AssemblerTester, InvalidAssignmentTargetThrowsTest) {
    // a + b = 3;   (잘못된 할당 대상)
    std::vector<Token> tokens = {
        { TokenType::IDENTIFIER, "a", 0},
        { TokenType::PLUS, "+", 0},
        { TokenType::IDENTIFIER, "b", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::NUMBER, "3", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    EXPECT_THROW(assembler.assemble(tokens), AssemblerError);
}

TEST_F(AssemblerTester, UnexpectedTokenThrowsTest) {
    // print * 5;   (표현식이 와야 할 자리에 엉뚱한 토큰)
    std::vector<Token> tokens = {
        { TokenType::PRINT, "print", 0},
        { TokenType::STAR, "*", 0},
        { TokenType::NUMBER, "5", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    EXPECT_THROW(assembler.assemble(tokens), AssemblerError);
}


TEST_F(AssemblerTester, ChainedAssignmentTest) {
    // a = b = 3;    // expect: right-associative, a = (b = 3)
    std::vector<Token> tokens = {
        { TokenType::IDENTIFIER, "a", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::IDENTIFIER, "b", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::NUMBER, "3", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression targetA({ tokens[0] }, "a");
    IdentifierExpression targetB({ tokens[2] }, "b");
    NumberExpression three({ tokens[4] }, 3);
    AssignExpression assignB({ tokens[3] }, &targetB, &three);
    AssignExpression assignA({ tokens[1] }, &targetA, &assignB);
    ExpressionStatement golden({ tokens[5] }, &assignA);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, AssignmentWithPrecedenceTest) {
    // a = 1 * 2 + 3 * 4;    // expect: a = ((1 * 2) + (3 * 4))
    std::vector<Token> tokens = {
        { TokenType::IDENTIFIER, "a", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::NUMBER, "1", 0},
        { TokenType::STAR, "*", 0},
        { TokenType::NUMBER, "2", 0},
        { TokenType::PLUS, "+", 0},
        { TokenType::NUMBER, "3", 0},
        { TokenType::STAR, "*", 0},
        { TokenType::NUMBER, "4", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression target({ tokens[0] }, "a");
    NumberExpression one({ tokens[2] }, 1);
    NumberExpression two({ tokens[4] }, 2);
    MultExpression mult1({ tokens[3] }, &one, &two);
    NumberExpression three({ tokens[6] }, 3);
    NumberExpression four({ tokens[8] }, 4);
    MultExpression mult2({ tokens[7] }, &three, &four);
    AddExpression add({ tokens[5] }, &mult1, &mult2);
    AssignExpression assign({ tokens[1] }, &target, &add);
    ExpressionStatement golden({ tokens[9] }, &assign);

    EXPECT_EQ(*root, golden);
}


TEST_F(AssemblerTester, MissingClosingParenInSubExpressionThrowsTest) {
    // print 1 * (2 + 3;   (닫는 괄호 누락, expect throw with msg "Expect ')' after expression.")
    std::vector<Token> tokens = {
        { TokenType::PRINT, "print", 0},
        { TokenType::NUMBER, "1", 0},
        { TokenType::STAR, "*", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::NUMBER, "2", 0},
        { TokenType::PLUS, "+", 0},
        { TokenType::NUMBER, "3", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    try {
        assembler.assemble(tokens);
        FAIL() << "Expected AssemblerError to be thrown.";
    } catch (const AssemblerError& error) {
        EXPECT_THAT(error.what(), HasSubstr("Expect ')' after expression."));
    }
}

TEST_F(AssemblerTester, MissingClosingBraceThrowsTest) {
    // { var x = 1;   (닫는 중괄호 누락, expect throw with msg "Expect '}' after block.")
    std::vector<Token> tokens = {
        { TokenType::LEFT_BRACE, "{", 0},
        { TokenType::VAR, "var", 0},
        { TokenType::IDENTIFIER, "x", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::NUMBER, "1", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    try {
        assembler.assemble(tokens);
        FAIL() << "Expected AssemblerError to be thrown.";
    } catch (const AssemblerError& error) {
        EXPECT_THAT(error.what(), HasSubstr("Expect '}' after block."));
    }
}

// ============================================================================
// 3일차 확장: postfix 체인 (call / field access / index)
// ============================================================================

TEST_F(AssemblerTester, CallExpressionTest) {
    // add(1, 2);
    std::vector<Token> tokens = {
        { TokenType::IDENTIFIER, "add", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::NUMBER, "1", 0},
        { TokenType::COMMA, ",", 0},
        { TokenType::NUMBER, "2", 0},
        { TokenType::RIGHT_PAREN, ")", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression callee({ tokens[0] }, "add");
    NumberExpression one({ tokens[2] }, 1);
    NumberExpression two({ tokens[4] }, 2);
    CallExpression call({ tokens[1], tokens[5] }, &callee, { &one, &two });
    ExpressionStatement golden({ tokens[6] }, &call);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, CallExpressionNoArgsTest) {
    // foo();
    std::vector<Token> tokens = {
        { TokenType::IDENTIFIER, "foo", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::RIGHT_PAREN, ")", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression callee({ tokens[0] }, "foo");
    CallExpression call({ tokens[1], tokens[2] }, &callee, {});
    ExpressionStatement golden({ tokens[3] }, &call);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, FieldAccessExpressionTest) {
    // r.speed;
    std::vector<Token> tokens = {
        { TokenType::IDENTIFIER, "r", 0},
        { TokenType::DOT, ".", 0},
        { TokenType::IDENTIFIER, "speed", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression object({ tokens[0] }, "r");
    FieldAccessExpression access({ tokens[1] }, &object, tokens[2]);
    ExpressionStatement golden({ tokens[3] }, &access);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, MethodCallExpressionTest) {
    // r.move(5);
    std::vector<Token> tokens = {
        { TokenType::IDENTIFIER, "r", 0},
        { TokenType::DOT, ".", 0},
        { TokenType::IDENTIFIER, "move", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::NUMBER, "5", 0},
        { TokenType::RIGHT_PAREN, ")", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression object({ tokens[0] }, "r");
    FieldAccessExpression access({ tokens[1] }, &object, tokens[2]);
    NumberExpression five({ tokens[4] }, 5);
    CallExpression call({ tokens[3], tokens[5] }, &access, { &five });
    ExpressionStatement golden({ tokens[6] }, &call);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, IndexExpressionTest) {
    // arr[0];
    std::vector<Token> tokens = {
        { TokenType::IDENTIFIER, "arr", 0},
        { TokenType::LEFT_BRACKET, "[", 0},
        { TokenType::NUMBER, "0", 0},
        { TokenType::RIGHT_BRACKET, "]", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression collection({ tokens[0] }, "arr");
    NumberExpression zero({ tokens[2] }, 0);
    IndexExpression index({ tokens[1], tokens[3] }, &collection, &zero);
    ExpressionStatement golden({ tokens[4] }, &index);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, ChainedPostfixTest) {
    // r.list[0]();
    std::vector<Token> tokens = {
        { TokenType::IDENTIFIER, "r", 0},
        { TokenType::DOT, ".", 0},
        { TokenType::IDENTIFIER, "list", 0},
        { TokenType::LEFT_BRACKET, "[", 0},
        { TokenType::NUMBER, "0", 0},
        { TokenType::RIGHT_BRACKET, "]", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::RIGHT_PAREN, ")", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression r({ tokens[0] }, "r");
    FieldAccessExpression list({ tokens[1] }, &r, tokens[2]);
    NumberExpression zero({ tokens[4] }, 0);
    IndexExpression index({ tokens[3], tokens[5] }, &list, &zero);
    CallExpression call({ tokens[6], tokens[7] }, &index, {});
    ExpressionStatement golden({ tokens[8] }, &call);

    EXPECT_EQ(*root, golden);
}

// ============================================================================
// 3일차 확장: This / Array / 대입 대상 일반화
// ============================================================================

TEST_F(AssemblerTester, ThisFieldAssignmentTest) {
    // This.speed = speed;
    std::vector<Token> tokens = {
        { TokenType::THIS, "This", 0},
        { TokenType::DOT, ".", 0},
        { TokenType::IDENTIFIER, "speed", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::IDENTIFIER, "speed", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    ThisExpression thisExpr({ tokens[0] });
    FieldAccessExpression target({ tokens[1] }, &thisExpr, tokens[2]);
    IdentifierExpression value({ tokens[4] }, "speed");
    AssignExpression assign({ tokens[3] }, &target, &value);
    ExpressionStatement golden({ tokens[5] }, &assign);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, ArrayExpressionTest) {
    // var a = Array(3);
    std::vector<Token> tokens = {
        { TokenType::VAR, "var", 0},
        { TokenType::IDENTIFIER, "a", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::ARRAY, "Array", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::NUMBER, "3", 0},
        { TokenType::RIGHT_PAREN, ")", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression a({ tokens[1] }, "a");
    NumberExpression three({ tokens[5] }, 3);
    ArrayExpression arrayExpr({ tokens[3] }, &three);
    DeclareStatement golden({ tokens[0], tokens[2], tokens[7] }, &a, &arrayExpr);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, IndexAssignmentTest) {
    // arr[0] = 5;
    std::vector<Token> tokens = {
        { TokenType::IDENTIFIER, "arr", 0},
        { TokenType::LEFT_BRACKET, "[", 0},
        { TokenType::NUMBER, "0", 0},
        { TokenType::RIGHT_BRACKET, "]", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::NUMBER, "5", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression collection({ tokens[0] }, "arr");
    NumberExpression zero({ tokens[2] }, 0);
    IndexExpression target({ tokens[1], tokens[3] }, &collection, &zero);
    NumberExpression five({ tokens[5] }, 5);
    AssignExpression assign({ tokens[4] }, &target, &five);
    ExpressionStatement golden({ tokens[6] }, &assign);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, FieldAssignmentTest) {
    // r.speed = 10;
    std::vector<Token> tokens = {
        { TokenType::IDENTIFIER, "r", 0},
        { TokenType::DOT, ".", 0},
        { TokenType::IDENTIFIER, "speed", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::NUMBER, "10", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression object({ tokens[0] }, "r");
    FieldAccessExpression target({ tokens[1] }, &object, tokens[2]);
    NumberExpression ten({ tokens[4] }, 10);
    AssignExpression assign({ tokens[3] }, &target, &ten);
    ExpressionStatement golden({ tokens[5] }, &assign);

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, InvalidAssignmentTargetNumberThrowsTest) {
    // 1 = 2;   (숫자 리터럴은 여전히 잘못된 대입 대상)
    std::vector<Token> tokens = {
        { TokenType::NUMBER, "1", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::NUMBER, "2", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    EXPECT_THROW(assembler.assemble(tokens), AssemblerError);
}

// ============================================================================
// 3일차 확장: 함수 선언 / return
// ============================================================================

TEST_F(AssemblerTester, FunctionDeclareStatementTest) {
    // Func add(a, b) { return a + b; }
    std::vector<Token> tokens = {
        { TokenType::FUNC, "Func", 0},
        { TokenType::IDENTIFIER, "add", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::IDENTIFIER, "a", 0},
        { TokenType::COMMA, ",", 0},
        { TokenType::IDENTIFIER, "b", 0},
        { TokenType::RIGHT_PAREN, ")", 0},
        { TokenType::LEFT_BRACE, "{", 0},
        { TokenType::RETURN, "return", 0},
        { TokenType::IDENTIFIER, "a", 0},
        { TokenType::PLUS, "+", 0},
        { TokenType::IDENTIFIER, "b", 0},
        { TokenType::SEMICOLON, ";", 0},
        { TokenType::RIGHT_BRACE, "}", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression aRef({ tokens[9] }, "a");
    IdentifierExpression bRef({ tokens[11] }, "b");
    AddExpression sum({ tokens[10] }, &aRef, &bRef);
    ReturnStatement ret({ tokens[8], tokens[12] }, &sum);
    FunctionDeclareStatement golden({ tokens[0], tokens[6], tokens[7], tokens[13] },
        tokens[1], { tokens[3], tokens[5] }, { &ret });

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, FunctionDeclareNoParamsTest) {
    // Func hello() { print "hi"; }
    std::vector<Token> tokens = {
        { TokenType::FUNC, "Func", 0},
        { TokenType::IDENTIFIER, "hello", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::RIGHT_PAREN, ")", 0},
        { TokenType::LEFT_BRACE, "{", 0},
        { TokenType::PRINT, "print", 0},
        { TokenType::STRING, "hi", 0},
        { TokenType::SEMICOLON, ";", 0},
        { TokenType::RIGHT_BRACE, "}", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    StringExpression hi({ tokens[6] }, "hi");
    PrintStatement printStmt({ tokens[5], tokens[7] }, &hi);
    FunctionDeclareStatement golden({ tokens[0], tokens[3], tokens[4], tokens[8] },
        tokens[1], {}, { &printStmt });

    EXPECT_EQ(*root, golden);
}

TEST_F(AssemblerTester, ReturnWithoutValueTest) {
    // Func noop() { return; }
    std::vector<Token> tokens = {
        { TokenType::FUNC, "Func", 0},
        { TokenType::IDENTIFIER, "noop", 0},
        { TokenType::LEFT_PAREN, "(", 0},
        { TokenType::RIGHT_PAREN, ")", 0},
        { TokenType::LEFT_BRACE, "{", 0},
        { TokenType::RETURN, "return", 0},
        { TokenType::SEMICOLON, ";", 0},
        { TokenType::RIGHT_BRACE, "}", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    ReturnStatement ret({ tokens[5], tokens[6] });
    FunctionDeclareStatement golden({ tokens[0], tokens[3], tokens[4], tokens[7] },
        tokens[1], {}, { &ret });

    EXPECT_EQ(*root, golden);
}

// ============================================================================
// 3일차 확장: 클래스 선언 (메서드는 Func 없이 파싱)
// ============================================================================

TEST_F(AssemblerTester, ClassDeclareStatementTest) {
    // Class Robot {
    //   init(speed) { This.speed = speed; }
    //   move(dist) { This.speed = This.speed + dist; }
    // }
    std::vector<Token> tokens = {
        { TokenType::CLASS, "Class", 0},          // 0
        { TokenType::IDENTIFIER, "Robot", 0},     // 1
        { TokenType::LEFT_BRACE, "{", 0},         // 2
        { TokenType::IDENTIFIER, "init", 0},      // 3
        { TokenType::LEFT_PAREN, "(", 0},         // 4
        { TokenType::IDENTIFIER, "speed", 0},     // 5
        { TokenType::RIGHT_PAREN, ")", 0},        // 6
        { TokenType::LEFT_BRACE, "{", 0},         // 7
        { TokenType::THIS, "This", 0},            // 8
        { TokenType::DOT, ".", 0},                // 9
        { TokenType::IDENTIFIER, "speed", 0},     // 10
        { TokenType::EQUAL, "=", 0},              // 11
        { TokenType::IDENTIFIER, "speed", 0},     // 12
        { TokenType::SEMICOLON, ";", 0},          // 13
        { TokenType::RIGHT_BRACE, "}", 0},        // 14
        { TokenType::IDENTIFIER, "move", 0},      // 15
        { TokenType::LEFT_PAREN, "(", 0},         // 16
        { TokenType::IDENTIFIER, "dist", 0},      // 17
        { TokenType::RIGHT_PAREN, ")", 0},        // 18
        { TokenType::LEFT_BRACE, "{", 0},         // 19
        { TokenType::THIS, "This", 0},            // 20
        { TokenType::DOT, ".", 0},                // 21
        { TokenType::IDENTIFIER, "speed", 0},     // 22
        { TokenType::EQUAL, "=", 0},              // 23
        { TokenType::THIS, "This", 0},            // 24
        { TokenType::DOT, ".", 0},                // 25
        { TokenType::IDENTIFIER, "speed", 0},     // 26
        { TokenType::PLUS, "+", 0},               // 27
        { TokenType::IDENTIFIER, "dist", 0},      // 28
        { TokenType::SEMICOLON, ";", 0},          // 29
        { TokenType::RIGHT_BRACE, "}", 0},        // 30
        { TokenType::RIGHT_BRACE, "}", 0},        // 31
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    ThisExpression thisInit({ tokens[8] });
    FieldAccessExpression initTarget({ tokens[9] }, &thisInit, tokens[10]);
    IdentifierExpression speedParam({ tokens[12] }, "speed");
    AssignExpression initAssign({ tokens[11] }, &initTarget, &speedParam);
    ExpressionStatement initBody({ tokens[13] }, &initAssign);
    MethodDeclareStatement initMethod({ tokens[6], tokens[7], tokens[14] },
        tokens[3], { tokens[5] }, { &initBody });

    ThisExpression thisMoveTarget({ tokens[20] });
    FieldAccessExpression moveTarget({ tokens[21] }, &thisMoveTarget, tokens[22]);
    ThisExpression thisMoveRead({ tokens[24] });
    FieldAccessExpression moveRead({ tokens[25] }, &thisMoveRead, tokens[26]);
    IdentifierExpression distRef({ tokens[28] }, "dist");
    AddExpression movePlus({ tokens[27] }, &moveRead, &distRef);
    AssignExpression moveAssign({ tokens[23] }, &moveTarget, &movePlus);
    ExpressionStatement moveBody({ tokens[29] }, &moveAssign);
    MethodDeclareStatement moveMethod({ tokens[18], tokens[19], tokens[30] },
        tokens[15], { tokens[17] }, { &moveBody });

    ClassDeclareStatement golden({ tokens[0], tokens[2], tokens[31] },
        tokens[1], { &initMethod, &moveMethod });

    EXPECT_EQ(*root, golden);
}

// ============================================================================
// 3일차 확장: instanceof
// ============================================================================

TEST_F(AssemblerTester, InstanceOfExpressionTest) {
    // r instanceof Robot;
    std::vector<Token> tokens = {
        { TokenType::IDENTIFIER, "r", 0},
        { TokenType::INSTANCEOF, "instanceof", 0},
        { TokenType::IDENTIFIER, "Robot", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression r({ tokens[0] }, "r");
    InstanceOfExpression instOf({ tokens[1] }, &r, tokens[2]);
    ExpressionStatement golden({ tokens[3] }, &instOf);

    EXPECT_EQ(*root, golden);
}

// ============================================================================
// 3일차 확장: import (Fake SourceReader 사용)
// ============================================================================

TEST(AssemblerImportTest, ImportStatementTest) {
    // lib.cf: var pi = 3;
    std::vector<Token> libTokens = {
        { TokenType::VAR, "var", 0},
        { TokenType::IDENTIFIER, "pi", 0},
        { TokenType::EQUAL, "=", 0},
        { TokenType::NUMBER, "3", 0},
        { TokenType::SEMICOLON, ";", 0},
    };
    FakeSourceReader fakeReader;
    fakeReader.files["lib.cf"] = libTokens;
    Assembler assembler(fakeReader);

    // import "lib.cf" alias math;
    std::vector<Token> tokens = {
        { TokenType::IMPORT, "import", 0},
        { TokenType::STRING, "lib.cf", 0},
        { TokenType::ALIAS, "alias", 0},
        { TokenType::IDENTIFIER, "math", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    auto tree = assembler.assemble(tokens);
    auto root = tree.getRoot();

    IdentifierExpression pi({ libTokens[1] }, "pi");
    NumberExpression three({ libTokens[3] }, 3);
    DeclareStatement declarePi({ libTokens[0], libTokens[2], libTokens[4] }, &pi, &three);
    ImportStatement golden({ tokens[0], tokens[4] }, tokens[3], { &declarePi });

    EXPECT_EQ(*root, golden);
}

TEST(AssemblerImportTest, CircularImportThrowsTest) {
    // a.cf: import "a.cf" alias x;   (자기 자신을 import)
    std::vector<Token> aTokens = {
        { TokenType::IMPORT, "import", 0},
        { TokenType::STRING, "a.cf", 0},
        { TokenType::ALIAS, "alias", 0},
        { TokenType::IDENTIFIER, "x", 0},
        { TokenType::SEMICOLON, ";", 0},
    };
    FakeSourceReader fakeReader;
    fakeReader.files["a.cf"] = aTokens;
    Assembler assembler(fakeReader);

    // import "a.cf" alias y;
    std::vector<Token> tokens = {
        { TokenType::IMPORT, "import", 0},
        { TokenType::STRING, "a.cf", 0},
        { TokenType::ALIAS, "alias", 0},
        { TokenType::IDENTIFIER, "y", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    EXPECT_THROW(assembler.assemble(tokens), AssemblerError);
}

TEST(AssemblerImportTest, FileNotFoundThrowsTest) {
    FakeSourceReader fakeReader; // 아무 파일도 등록하지 않음
    Assembler assembler(fakeReader);

    // import "missing.cf" alias m;
    std::vector<Token> tokens = {
        { TokenType::IMPORT, "import", 0},
        { TokenType::STRING, "missing.cf", 0},
        { TokenType::ALIAS, "alias", 0},
        { TokenType::IDENTIFIER, "m", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    EXPECT_THROW(assembler.assemble(tokens), AssemblerError);
}

TEST(AssemblerImportTest, NonDeclarationInsideImportThrowsTest) {
    // bad.cf: print 1;   (선언이 아닌 문장은 import 대상으로 허용하지 않음)
    std::vector<Token> badTokens = {
        { TokenType::PRINT, "print", 0},
        { TokenType::NUMBER, "1", 0},
        { TokenType::SEMICOLON, ";", 0},
    };
    FakeSourceReader fakeReader;
    fakeReader.files["bad.cf"] = badTokens;
    Assembler assembler(fakeReader);

    // import "bad.cf" alias b;
    std::vector<Token> tokens = {
        { TokenType::IMPORT, "import", 0},
        { TokenType::STRING, "bad.cf", 0},
        { TokenType::ALIAS, "alias", 0},
        { TokenType::IDENTIFIER, "b", 0},
        { TokenType::SEMICOLON, ";", 0},
    };

    EXPECT_THROW(assembler.assemble(tokens), AssemblerError);
}
