#include <sstream>

#include "gmock/gmock.h"
#include "Executor.h"
#include "../Assembler/SyntaxTree.h"

// Variables, assignment, and block scope. None of DeclareStatement /
// IdentifierExpression / AssignExpression / BlockStatement have handlers
// registered yet, so these all fail (red) until that work lands.

TEST(ExecutorVariableTest, DeclareAndUseVariables) {
    // var a = 10; var b = 20; print a + b; // expect: 30
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression aIdent({}, "a");
    NumberExpression ten({}, 10);
    DeclareStatement declareA({}, &aIdent, &ten);

    IdentifierExpression bIdent({}, "b");
    NumberExpression twenty({}, 20);
    DeclareStatement declareB({}, &bIdent, &twenty);

    IdentifierExpression aRef({}, "a");
    IdentifierExpression bRef({}, "b");
    AddExpression sum({}, &aRef, &bRef);
    PrintStatement printSum({}, &sum);

    BlockStatement program({}, std::vector<Statement*>{ &declareA, &declareB, &printSum });

    executor.execute(&program);

    EXPECT_EQ(out.str(), "30\n");
}

TEST(ExecutorVariableTest, ReassignExistingVariable) {
    // var a = 10; a = a + 5; print a; // expect: 15
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression aIdent({}, "a");
    NumberExpression ten({}, 10);
    DeclareStatement declareA({}, &aIdent, &ten);

    IdentifierExpression aRefForAdd({}, "a");
    NumberExpression five({}, 5);
    AddExpression aPlusFive({}, &aRefForAdd, &five);
    IdentifierExpression aAssignTarget({}, "a");
    AssignExpression reassign({}, &aAssignTarget, &aPlusFive);
    ExpressionStatement reassignStmt({}, &reassign);

    IdentifierExpression aRefForPrint({}, "a");
    PrintStatement printA({}, &aRefForPrint);

    BlockStatement program({}, std::vector<Statement*>{ &declareA, &reassignStmt, &printA });

    executor.execute(&program);

    EXPECT_EQ(out.str(), "15\n");
}

TEST(ExecutorVariableTest, BlockScope_ShadowsOuterVariable) {
    // var x = "global"; { var x = "inner"; print x; } print x;
    // expect: inner \n global
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression xOuterIdent({}, "x");
    StringExpression globalStr({}, "global");
    DeclareStatement declareOuterX({}, &xOuterIdent, &globalStr);

    IdentifierExpression xInnerIdent({}, "x");
    StringExpression innerStr({}, "inner");
    DeclareStatement declareInnerX({}, &xInnerIdent, &innerStr);

    IdentifierExpression xRefInner({}, "x");
    PrintStatement printInnerX({}, &xRefInner);

    BlockStatement innerBlock({}, std::vector<Statement*>{ &declareInnerX, &printInnerX });

    IdentifierExpression xRefOuter({}, "x");
    PrintStatement printOuterX({}, &xRefOuter);

    BlockStatement program({}, std::vector<Statement*>{ &declareOuterX, &innerBlock, &printOuterX });

    executor.execute(&program);

    EXPECT_EQ(out.str(), "inner\nglobal\n");
}

TEST(ExecutorVariableTest, InnerBlockCanModifyOuterVariable) {
    // var count = 0; { count = count + 1; } print count; // expect: 1
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression countIdent({}, "count");
    NumberExpression zero({}, 0);
    DeclareStatement declareCount({}, &countIdent, &zero);

    IdentifierExpression countRefForAdd({}, "count");
    NumberExpression one({}, 1);
    AddExpression countPlusOne({}, &countRefForAdd, &one);
    IdentifierExpression countAssignTarget({}, "count");
    AssignExpression assignCount({}, &countAssignTarget, &countPlusOne);
    ExpressionStatement assignStmt({}, &assignCount);

    BlockStatement innerBlock({}, std::vector<Statement*>{ &assignStmt });

    IdentifierExpression countRefForPrint({}, "count");
    PrintStatement printCount({}, &countRefForPrint);

    BlockStatement program({}, std::vector<Statement*>{ &declareCount, &innerBlock, &printCount });

    executor.execute(&program);

    EXPECT_EQ(out.str(), "1\n");
}

TEST(ExecutorVariableTest, NestedBlocks_ResolveVariablesFromAnyEnclosingScope) {
    // var outer = "A"; { var inner = "B"; { print outer + inner; } }
    // expect: AB
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression outerIdent({}, "outer");
    StringExpression aStr({}, "A");
    DeclareStatement declareOuter({}, &outerIdent, &aStr);

    IdentifierExpression innerIdent({}, "inner");
    StringExpression bStr({}, "B");
    DeclareStatement declareInner({}, &innerIdent, &bStr);

    IdentifierExpression outerRef({}, "outer");
    IdentifierExpression innerRef({}, "inner");
    AddExpression concat({}, &outerRef, &innerRef);
    PrintStatement printConcat({}, &concat);

    BlockStatement innermostBlock({}, std::vector<Statement*>{ &printConcat });
    BlockStatement middleBlock({}, std::vector<Statement*>{ &declareInner, &innermostBlock });
    BlockStatement program({}, std::vector<Statement*>{ &declareOuter, &middleBlock });

    executor.execute(&program);

    EXPECT_EQ(out.str(), "AB\n");
}
