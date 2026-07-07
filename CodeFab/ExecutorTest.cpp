#include <sstream>

#include "gmock/gmock.h"
#include "Executor.h"
#include "MockSyntaxTree.h"

struct ExecutorTester : public testing::Test {
    Executor executor;
};

TEST_F(ExecutorTester, Evaluate_NumberLiteral_ReturnsItsValue) {
    NumberExpression number(1);

    Value result = executor.evaluate(&number);

    EXPECT_EQ(result.asNumber(), 1.0);
}

TEST_F(ExecutorTester, Evaluate_AddExpression_ReturnsSum) {
    NumberExpression one(1);
    NumberExpression two(2);
    AddExpression add(&one, &two);

    Value result = executor.evaluate(&add);

    EXPECT_EQ(result.asNumber(), 3.0);
}

TEST_F(ExecutorTester, Evaluate_MultExpression_ReturnsProduct) {
    NumberExpression two(2);
    NumberExpression three(3);
    MultExpression mult(&two, &three);

    Value result = executor.evaluate(&mult);

    EXPECT_EQ(result.asNumber(), 6.0);
}

// Goal case: print 1 + 2 * 3; // expect: 7
TEST(ExecutorPrintTest, Execute_PrintStatement_WritesEvaluatedValueToOutput) {
    std::ostringstream out;
    Executor executor(out);

    NumberExpression one(1);
    NumberExpression two(2);
    NumberExpression three(3);
    MultExpression mult(&two, &three);
    AddExpression add(&one, &mult);
    PrintStatement print(&add);

    executor.execute(&print);

    EXPECT_EQ(out.str(), "7\n");
}

// The real entry point: Executor receives a SyntaxTree and must start
// executing from getRoot(), not from a hand-picked node.
TEST(ExecutorRunTest, Run_ExecutesFromTreeRoot) {
    std::ostringstream out;
    Executor executor(out);

    SyntaxTree tree;
    auto one = std::make_unique<NumberExpression>(1);
    auto two = std::make_unique<NumberExpression>(2);
    auto three = std::make_unique<NumberExpression>(3);
    auto mult = std::make_unique<MultExpression>(two.get(), three.get());
    auto add = std::make_unique<AddExpression>(one.get(), mult.get());
    auto print = std::make_unique<PrintStatement>(add.get());

    tree.setRoot(print.get());
    tree.add(std::move(one));
    tree.add(std::move(two));
    tree.add(std::move(three));
    tree.add(std::move(mult));
    tree.add(std::move(add));
    tree.add(std::move(print));

    executor.run(tree);

    EXPECT_EQ(out.str(), "7\n");
}
