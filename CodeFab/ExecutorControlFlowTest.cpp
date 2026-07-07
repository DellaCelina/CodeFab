#include <sstream>

#include "gmock/gmock.h"
#include "Executor.h"
#include "SyntaxTree.h"

// if/else and for. IfStatement/ForStatement have no handlers registered
// yet, so these all fail (red) until that work lands.

TEST(ExecutorControlFlowTest, If_TrueCondition_ExecutesThenBranch) {
    // if (true) print "bbq"; // expect: bbq
    std::ostringstream out;
    Executor executor(out);

    BooleanExpression condition({}, true);
    StringExpression bbq({}, "bbq");
    PrintStatement printBbq({}, &bbq);
    IfStatement ifStmt({}, &condition, &printBbq);

    executor.execute(&ifStmt);

    EXPECT_EQ(out.str(), "bbq\n");
}

TEST(ExecutorControlFlowTest, If_FalseCondition_ExecutesElseBranch) {
    // if (false) print "no"; else print "kfc"; // expect: kfc
    std::ostringstream out;
    Executor executor(out);

    BooleanExpression condition({}, false);
    StringExpression no({}, "no");
    PrintStatement printNo({}, &no);
    StringExpression kfc({}, "kfc");
    PrintStatement printKfc({}, &kfc);
    IfStatement ifStmt({}, &condition, &printNo, &printKfc);

    executor.execute(&ifStmt);

    EXPECT_EQ(out.str(), "kfc\n");
}

TEST(ExecutorControlFlowTest, If_FalseConditionWithNoElse_ExecutesNothing) {
    // if (false) print "no"; // expect: (아무 출력도 없음)
    std::ostringstream out;
    Executor executor(out);

    BooleanExpression condition({}, false);
    StringExpression no({}, "no");
    PrintStatement printNo({}, &no);
    IfStatement ifStmt({}, &condition, &printNo);

    executor.execute(&ifStmt);

    EXPECT_EQ(out.str(), "");
}

TEST(ExecutorControlFlowTest, NestedIf_ElseBindsToNearestIf) {
    // if (true) { if (false) print "kfc"; else print "bbq"; } // expect: bbq
    std::ostringstream out;
    Executor executor(out);

    BooleanExpression innerCondition({}, false);
    StringExpression kfc({}, "kfc");
    PrintStatement printKfc({}, &kfc);
    StringExpression bbq({}, "bbq");
    PrintStatement printBbq({}, &bbq);
    IfStatement innerIf({}, &innerCondition, &printKfc, &printBbq);

    BlockStatement outerBlock({}, std::vector<Statement*>{ &innerIf });

    BooleanExpression outerCondition({}, true);
    IfStatement outerIf({}, &outerCondition, &outerBlock);

    executor.execute(&outerIf);

    EXPECT_EQ(out.str(), "bbq\n");
}

TEST(ExecutorControlFlowTest, For_LoopsWhileConditionHolds) {
    // var j = -1; for (j = 0; j < 3; j = j + 1) { print j; } // expect: 0\n1\n2\n
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression jDeclareIdent({}, "j");
    NumberExpression placeholder({}, -1);
    DeclareStatement declareJ({}, &jDeclareIdent, &placeholder);

    IdentifierExpression jInitTarget({}, "j");
    NumberExpression zero({}, 0);
    AssignExpression init({}, &jInitTarget, &zero);

    IdentifierExpression jCompareRef({}, "j");
    NumberExpression three({}, 3);
    LessExpression compare({}, &jCompareRef, &three);

    IdentifierExpression jNextRef({}, "j");
    NumberExpression one({}, 1);
    AddExpression jPlusOne({}, &jNextRef, &one);
    IdentifierExpression jNextTarget({}, "j");
    AssignExpression next({}, &jNextTarget, &jPlusOne);

    IdentifierExpression jPrintRef({}, "j");
    PrintStatement printJ({}, &jPrintRef);
    BlockStatement loopBody({}, std::vector<Statement*>{ &printJ });

    ForStatement forLoop({}, &init, &compare, &next, &loopBody);

    BlockStatement program({}, std::vector<Statement*>{ &declareJ, &forLoop });

    executor.execute(&program);

    EXPECT_EQ(out.str(), "0\n1\n2\n");
}

TEST(ExecutorControlFlowTest, For_ConditionFalseFromStart_NeverExecutesBody) {
    // var j = 5; for (j = 5; j < 3; j = j + 1) { print j; } // expect: (아무 출력도 없음)
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression jDeclareIdent({}, "j");
    NumberExpression five({}, 5);
    DeclareStatement declareJ({}, &jDeclareIdent, &five);

    IdentifierExpression jInitTarget({}, "j");
    NumberExpression fiveAgain({}, 5);
    AssignExpression init({}, &jInitTarget, &fiveAgain);

    IdentifierExpression jCompareRef({}, "j");
    NumberExpression three({}, 3);
    LessExpression compare({}, &jCompareRef, &three);

    IdentifierExpression jNextRef({}, "j");
    NumberExpression one({}, 1);
    AddExpression jPlusOne({}, &jNextRef, &one);
    IdentifierExpression jNextTarget({}, "j");
    AssignExpression next({}, &jNextTarget, &jPlusOne);

    IdentifierExpression jPrintRef({}, "j");
    PrintStatement printJ({}, &jPrintRef);
    BlockStatement loopBody({}, std::vector<Statement*>{ &printJ });

    ForStatement forLoop({}, &init, &compare, &next, &loopBody);

    BlockStatement program({}, std::vector<Statement*>{ &declareJ, &forLoop });

    executor.execute(&program);

    EXPECT_EQ(out.str(), "");
}
