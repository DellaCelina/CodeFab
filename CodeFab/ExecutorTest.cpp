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
