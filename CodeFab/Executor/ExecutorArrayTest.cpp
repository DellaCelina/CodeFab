#include <sstream>
#include <vector>

#include "gmock/gmock.h"
#include "Executor.h"
#include "../Assembler/SyntaxTree.h"

// 정적 배열: 생성, index 읽기/쓰기, 런타임 에러.

TEST(ExecutorArrayTest, ArrayCreation_FillsWithNil) {
    // var arr = Array(3);
    // print arr[0]; // expect: nil
    std::ostringstream out;
    Executor executor(out);

    NumberExpression size({}, 3);
    ArrayExpression arrayExpr({}, &size);
    IdentifierExpression arrIdent({}, "arr");
    DeclareStatement declareArr({}, &arrIdent, &arrayExpr);
    executor.execute(&declareArr);

    IdentifierExpression arrRef({}, "arr");
    NumberExpression zero({}, 0);
    IndexExpression readZero({}, &arrRef, &zero);
    PrintStatement printResult({}, &readZero);

    executor.execute(&printResult);

    EXPECT_EQ(out.str(), "nil\n");
}

TEST(ExecutorArrayTest, IndexWrite_ThenReadReturnsStoredValue) {
    // var arr = Array(5);
    // arr[0] = 3;
    // print arr[0]; // expect: 3
    std::ostringstream out;
    Executor executor(out);

    NumberExpression size({}, 5);
    ArrayExpression arrayExpr({}, &size);
    IdentifierExpression arrIdent({}, "arr");
    DeclareStatement declareArr({}, &arrIdent, &arrayExpr);
    executor.execute(&declareArr);

    IdentifierExpression arrRefForWrite({}, "arr");
    NumberExpression zeroForWrite({}, 0);
    IndexExpression writeTarget({}, &arrRefForWrite, &zeroForWrite);
    NumberExpression three({}, 3);
    AssignExpression assignZero({}, &writeTarget, &three);
    ExpressionStatement assignStmt({}, &assignZero);
    executor.execute(&assignStmt);

    IdentifierExpression arrRefForRead({}, "arr");
    NumberExpression zeroForRead({}, 0);
    IndexExpression readTarget({}, &arrRefForRead, &zeroForRead);
    PrintStatement printResult({}, &readTarget);

    executor.execute(&printResult);

    EXPECT_EQ(out.str(), "3\n");
}

TEST(ExecutorArrayTest, ArraySize_NotANumber_ThrowsExecutorError) {
    std::ostringstream out;
    Executor executor(out);

    StringExpression sizeAsString({}, "5");
    ArrayExpression arrayExpr({}, &sizeAsString);

    EXPECT_THROW(executor.evaluate(&arrayExpr), ExecutorError);
}

TEST(ExecutorArrayTest, IndexOnNonArray_ThrowsExecutorError) {
    std::ostringstream out;
    Executor executor(out);

    NumberExpression notAnArray({}, 1);
    NumberExpression zero({}, 0);
    IndexExpression index({}, &notAnArray, &zero);

    EXPECT_THROW(executor.evaluate(&index), ExecutorError);
}

TEST(ExecutorArrayTest, IndexNotANumber_ThrowsExecutorError) {
    std::ostringstream out;
    Executor executor(out);

    NumberExpression size({}, 3);
    ArrayExpression arrayExpr({}, &size);
    StringExpression badIndex({}, "zero");
    IndexExpression index({}, &arrayExpr, &badIndex);

    EXPECT_THROW(executor.evaluate(&index), ExecutorError);
}

TEST(ExecutorArrayTest, IndexOutOfRange_ThrowsExecutorError) {
    std::ostringstream out;
    Executor executor(out);

    NumberExpression size({}, 3);
    ArrayExpression arrayExpr({}, &size);
    NumberExpression outOfRange({}, 3);  // 유효 범위는 0..2
    IndexExpression index({}, &arrayExpr, &outOfRange);

    EXPECT_THROW(executor.evaluate(&index), ExecutorError);
}
