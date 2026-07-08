#include <sstream>
#include <vector>

#include "gmock/gmock.h"
#include "Executor.h"
#include "../Assembler/SyntaxTree.h"

// Func 선언/호출, return, 재귀 호출.

TEST(ExecutorFunctionTest, DeclareAndCall_ReturnsSum) {
    // Func add(a, b) { return a + b; }
    // print add(3, 5); // expect: 8
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression aRef({}, "a");
    IdentifierExpression bRef({}, "b");
    AddExpression sum({}, &aRef, &bRef);
    ReturnStatement returnSum({}, &sum);
    std::vector<Statement*> body{ &returnSum };
    std::vector<Token> params{
        Token{ TokenType::IDENTIFIER, "a", 0 },
        Token{ TokenType::IDENTIFIER, "b", 0 },
    };
    Token nameToken{ TokenType::IDENTIFIER, "add", 0 };
    FunctionDeclareStatement addDecl({}, nameToken, params, body);

    executor.execute(&addDecl);

    IdentifierExpression calleeRef({}, "add");
    NumberExpression three({}, 3);
    NumberExpression five({}, 5);
    std::vector<Expression*> args{ &three, &five };
    CallExpression callAdd({}, &calleeRef, args);
    PrintStatement printCall({}, &callAdd);

    executor.execute(&printCall);

    EXPECT_EQ(out.str(), "8\n");
}

TEST(ExecutorFunctionTest, RecursiveCall_ComputesFactorial) {
    // Func fact(n) {
    //   if (n <= 1) { return 1; }
    //   return n * fact(n - 1);
    // }
    // print fact(5); // expect: 120
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression nRefForIf({}, "n");
    NumberExpression one({}, 1);
    LessEqualExpression nLeOne({}, &nRefForIf, &one);
    NumberExpression oneToReturn({}, 1);
    ReturnStatement returnOne({}, &oneToReturn);
    std::vector<Statement*> thenBlockStmts{ &returnOne };
    BlockStatement thenBlock({}, thenBlockStmts);
    IfStatement baseCaseIf({}, &nLeOne, &thenBlock);

    IdentifierExpression nRefForMult({}, "n");
    IdentifierExpression factRef({}, "fact");
    IdentifierExpression nRefForArg({}, "n");
    NumberExpression oneToSubtract({}, 1);
    SubExpression nMinusOne({}, &nRefForArg, &oneToSubtract);
    std::vector<Expression*> factArgs{ &nMinusOne };
    CallExpression recursiveCall({}, &factRef, factArgs);
    MultExpression nTimesFact({}, &nRefForMult, &recursiveCall);
    ReturnStatement returnRecursive({}, &nTimesFact);

    std::vector<Statement*> body{ &baseCaseIf, &returnRecursive };
    std::vector<Token> params{ Token{ TokenType::IDENTIFIER, "n", 0 } };
    Token nameToken{ TokenType::IDENTIFIER, "fact", 0 };
    FunctionDeclareStatement factDecl({}, nameToken, params, body);

    executor.execute(&factDecl);

    IdentifierExpression calleeRef({}, "fact");
    NumberExpression five({}, 5);
    std::vector<Expression*> callArgs{ &five };
    CallExpression callFact({}, &calleeRef, callArgs);
    PrintStatement printResult({}, &callFact);

    executor.execute(&printResult);

    EXPECT_EQ(out.str(), "120\n");
}

TEST(ExecutorFunctionTest, CallWithWrongArgumentCount_ThrowsExecutorError) {
    std::ostringstream out;
    Executor executor(out);

    std::vector<Statement*> body;
    std::vector<Token> params{ Token{ TokenType::IDENTIFIER, "a", 0 } };
    Token nameToken{ TokenType::IDENTIFIER, "oneArg", 0 };
    FunctionDeclareStatement decl({}, nameToken, params, body);
    executor.execute(&decl);

    IdentifierExpression calleeRef({}, "oneArg");
    std::vector<Expression*> noArgs;
    CallExpression call({}, &calleeRef, noArgs);

    EXPECT_THROW(executor.evaluate(&call), ExecutorError);
}

TEST(ExecutorFunctionTest, CallingNonCallableValue_ThrowsExecutorError) {
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression xIdent({}, "x");
    NumberExpression one({}, 1);
    DeclareStatement declareX({}, &xIdent, &one);
    executor.execute(&declareX);

    IdentifierExpression calleeRef({}, "x");
    std::vector<Expression*> noArgs;
    CallExpression call({}, &calleeRef, noArgs);

    EXPECT_THROW(executor.evaluate(&call), ExecutorError);
}

TEST(ExecutorFunctionTest, Parameters_DoNotLeakToOuterScope) {
    // Func identity(value) { return value; }
    // identity(42);
    // print value; // expect: ExecutorError (정의되지 않은 변수)
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression valueRef({}, "value");
    ReturnStatement ret({}, &valueRef);
    std::vector<Statement*> body{ &ret };
    std::vector<Token> params{ Token{ TokenType::IDENTIFIER, "value", 0 } };
    Token nameToken{ TokenType::IDENTIFIER, "identity", 0 };
    FunctionDeclareStatement decl({}, nameToken, params, body);
    executor.execute(&decl);

    IdentifierExpression calleeRef({}, "identity");
    NumberExpression arg({}, 42);
    std::vector<Expression*> args{ &arg };
    CallExpression call({}, &calleeRef, args);
    executor.evaluate(&call);

    IdentifierExpression leakedRef({}, "value");
    EXPECT_THROW(executor.evaluate(&leakedRef), ExecutorError);
}

TEST(ExecutorFunctionTest, CallWithoutReturn_YieldsNil) {
    // Func noop() { 1; }
    // print noop(); // expect: nil
    std::ostringstream out;
    Executor executor(out);

    NumberExpression unused({}, 1);
    ExpressionStatement noopBody({}, &unused);
    std::vector<Statement*> body{ &noopBody };
    std::vector<Token> params;
    Token nameToken{ TokenType::IDENTIFIER, "noop", 0 };
    FunctionDeclareStatement decl({}, nameToken, params, body);
    executor.execute(&decl);

    IdentifierExpression calleeRef({}, "noop");
    std::vector<Expression*> args;
    CallExpression call({}, &calleeRef, args);
    PrintStatement printResult({}, &call);

    executor.execute(&printResult);

    EXPECT_EQ(out.str(), "nil\n");
}
