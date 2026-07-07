#include <sstream>
#include <stdexcept>

#include "gmock/gmock.h"
#include "Executor.h"
#include "SyntaxTree.h"

struct ExecutorTester : public testing::Test {
    Executor executor;
};

TEST_F(ExecutorTester, Evaluate_NumberLiteral_ReturnsItsValue) {
    NumberExpression number({}, 1);

    Value result = executor.evaluate(&number);

    EXPECT_EQ(result.asNumber(), 1.0);
}

TEST_F(ExecutorTester, Evaluate_AddExpression_ReturnsSum) {
    NumberExpression one({}, 1);
    NumberExpression two({}, 2);
    AddExpression add({}, &one, &two);

    Value result = executor.evaluate(&add);

    EXPECT_EQ(result.asNumber(), 3.0);
}

TEST_F(ExecutorTester, Evaluate_MultExpression_ReturnsProduct) {
    NumberExpression two({}, 2);
    NumberExpression three({}, 3);
    MultExpression mult({}, &two, &three);

    Value result = executor.evaluate(&mult);

    EXPECT_EQ(result.asNumber(), 6.0);
}

TEST_F(ExecutorTester, Evaluate_SubExpression_LeftAssociative) {
    // 10 - 4 - 3 == 3
    NumberExpression ten({}, 10);
    NumberExpression four({}, 4);
    NumberExpression three({}, 3);
    SubExpression tenMinusFour({}, &ten, &four);
    SubExpression result({}, &tenMinusFour, &three);

    EXPECT_EQ(executor.evaluate(&result).asNumber(), 3.0);
}

TEST_F(ExecutorTester, Evaluate_DivideExpression_LeftAssociative) {
    // 8 / 2 / 2 == 2
    NumberExpression eight({}, 8);
    NumberExpression two({}, 2);
    NumberExpression twoAgain({}, 2);
    DivideExpression eightDivTwo({}, &eight, &two);
    DivideExpression result({}, &eightDivTwo, &twoAgain);

    EXPECT_EQ(executor.evaluate(&result).asNumber(), 2.0);
}

TEST_F(ExecutorTester, Evaluate_ParenthesizedExpression_OverridesPrecedence) {
    // (1 + 2) * 3 == 9
    NumberExpression one({}, 1);
    NumberExpression two({}, 2);
    NumberExpression three({}, 3);
    AddExpression onePlusTwo({}, &one, &two);
    MultExpression result({}, &onePlusTwo, &three);

    EXPECT_EQ(executor.evaluate(&result).asNumber(), 9.0);
}

TEST_F(ExecutorTester, Evaluate_NegativeExpression_NegatesOperand) {
    // -3 + 2 == -1
    NumberExpression three({}, 3);
    NegativeExpression negThree({}, &three);
    NumberExpression two({}, 2);
    AddExpression result({}, &negThree, &two);

    EXPECT_EQ(executor.evaluate(&result).asNumber(), -1.0);
}

TEST_F(ExecutorTester, Evaluate_LessExpression_ReturnsBoolean) {
    NumberExpression one({}, 1);
    NumberExpression two({}, 2);
    LessExpression less({}, &one, &two);

    EXPECT_EQ(executor.evaluate(&less).asBoolean(), true);
}

TEST_F(ExecutorTester, Evaluate_GreaterExpression_ReturnsBoolean) {
    NumberExpression three({}, 3);
    NumberExpression five({}, 5);
    GreaterExpression greater({}, &three, &five);

    EXPECT_EQ(executor.evaluate(&greater).asBoolean(), false);
}

TEST_F(ExecutorTester, Evaluate_EqualExpression_ReturnsBoolean) {
    NumberExpression three({}, 3);
    NumberExpression threeAgain({}, 3);
    EqualExpression equal({}, &three, &threeAgain);

    EXPECT_EQ(executor.evaluate(&equal).asBoolean(), true);
}

TEST_F(ExecutorTester, Evaluate_NotEqualExpression_ReturnsBoolean) {
    NumberExpression three({}, 3);
    NumberExpression five({}, 5);
    NotEqualExpression notEqual({}, &three, &five);

    EXPECT_EQ(executor.evaluate(&notEqual).asBoolean(), true);
}

TEST_F(ExecutorTester, Evaluate_LessEqualExpression_ReturnsBoolean) {
    NumberExpression three({}, 3);
    NumberExpression threeAgain({}, 3);
    LessEqualExpression lessEqual({}, &three, &threeAgain);

    EXPECT_EQ(executor.evaluate(&lessEqual).asBoolean(), true);
}

TEST_F(ExecutorTester, Evaluate_GreaterEqualExpression_ReturnsBoolean) {
    NumberExpression five({}, 5);
    NumberExpression three({}, 3);
    GreaterEqualExpression greaterEqual({}, &five, &three);

    EXPECT_EQ(executor.evaluate(&greaterEqual).asBoolean(), true);
}

TEST_F(ExecutorTester, Evaluate_AddExpression_ConcatenatesStrings) {
    StringExpression hello({}, "Hello, ");
    StringExpression codefab({}, "CodeFab!");
    AddExpression add({}, &hello, &codefab);

    EXPECT_EQ(executor.evaluate(&add).asString(), "Hello, CodeFab!");
}

TEST_F(ExecutorTester, Evaluate_BooleanLiteral_ReturnsItsValue) {
    BooleanExpression trueLiteral({}, true);
    BooleanExpression falseLiteral({}, false);

    EXPECT_EQ(executor.evaluate(&trueLiteral).asBoolean(), true);
    EXPECT_EQ(executor.evaluate(&falseLiteral).asBoolean(), false);
}

TEST_F(ExecutorTester, Evaluate_NumberLiteral_FormatsWithoutTrailingZero) {
    NumberExpression five({}, 5.0);
    NumberExpression pi({}, 3.14);

    EXPECT_EQ(executor.evaluate(&five).toString(), "5");
    EXPECT_EQ(executor.evaluate(&pi).toString(), "3.14");
}

// Goal case: print 1 + 2 * 3; // expect: 7
TEST(ExecutorPrintTest, Execute_PrintStatement_WritesEvaluatedValueToOutput) {
    std::ostringstream out;
    Executor executor(out);

    NumberExpression one({}, 1);
    NumberExpression two({}, 2);
    NumberExpression three({}, 3);
    MultExpression mult({}, &two, &three);
    AddExpression add({}, &one, &mult);
    PrintStatement print({}, &add);

    executor.execute(&print);

    EXPECT_EQ(out.str(), "7\n");
}

// The real entry point: Executor implements ExecuteInterface and must start
// executing from the tree's getRoot(), not from a hand-picked node.
TEST(ExecutorPrintTest, Execute_SyntaxTree_ExecutesFromRoot) {
    std::ostringstream out;
    Executor executor(out);

    SyntaxTree tree;
    auto one = std::make_unique<NumberExpression>(std::vector<Token>{}, 1);
    auto two = std::make_unique<NumberExpression>(std::vector<Token>{}, 2);
    auto three = std::make_unique<NumberExpression>(std::vector<Token>{}, 3);
    auto mult = std::make_unique<MultExpression>(std::vector<Token>{}, two.get(), three.get());
    auto add = std::make_unique<AddExpression>(std::vector<Token>{}, one.get(), mult.get());
    auto print = std::make_unique<PrintStatement>(std::vector<Token>{}, add.get());

    tree.setRoot(print.get());
    tree.add(std::move(one));
    tree.add(std::move(two));
    tree.add(std::move(three));
    tree.add(std::move(mult));
    tree.add(std::move(add));
    tree.add(std::move(print));

    executor.execute(tree);

    EXPECT_EQ(out.str(), "7\n");
}

TEST_F(ExecutorTester, Execute_SyntaxTree_ThrowsIfRootIsNotStatement) {
    SyntaxTree tree;
    NumberExpression number({}, 1);
    tree.setRoot(&number);

    EXPECT_THROW(executor.execute(tree), std::logic_error);
}
