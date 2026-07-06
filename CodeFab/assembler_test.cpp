#include <vector>

#include "gmock/gmock.h"
#include "assembler.h"

using namespace testing;

struct AssemblerTester : public Test {
    Assembler assembler;
};

TEST_F(AssemblerTester, PrintWithExpressionTest) {
    /*
    print 1 + 2 * 3;        // expect: 7

    PrintStatement
      - expr: BinaryExpression
        - left: NumberExpression
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

    auto root = tree->getRoot();

    NumberExpression one({ tokens[1] }, 1);
    NumberExpression two({ tokens[3] }, 2);
    NumberExpression three({ tokens[5] }, 3);
    MultExpression mult({ tokens[3], tokens[4], tokens[5] }, &two, &three);
    AddExpression add({ tokens[1], tokens[2], tokens[3], tokens[4], tokens[5] }, &one, &mult);
    PrintStatement golden(tokens, &add);

    EXPECT_EQ(*root, golden);
}




