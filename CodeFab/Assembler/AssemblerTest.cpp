#include <vector>

#include "gmock/gmock.h"
#include "Assembler.h"
#include "FileSourceReader.h"
#include "../Tokenizer/Tokenizer.h"

using namespace testing;

struct AssemblerTester : public Test {
    Tokenizer tokenizer;
    FileSourceReader sourceReader;
    Assembler assembler{ tokenizer, sourceReader };
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
