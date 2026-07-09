#include <sstream>
#include <vector>

#include "gmock/gmock.h"
#include "Executor.h"
#include "../Assembler/SyntaxTree.h"

// Class 선언/인스턴스화, 필드 읽기/쓰기, 메서드 호출, This, init 생성자.

TEST(ExecutorClassTest, InitSetsField_GetterMethodReturnsIt) {
    // Class Robot { init(name) { This.name = name; } getName() { return This.name; } }
    // var r = Robot("ABC");
    // print r.getName(); // expect: ABC
    std::ostringstream out;
    Executor executor(out);

    ThisExpression thisForInit(std::vector<Token>{});
    IdentifierExpression nameParamRef({}, "name");
    FieldAccessExpression nameFieldForInit({}, &thisForInit, Token{ TokenType::IDENTIFIER, "name", 0 });
    AssignExpression assignName({}, &nameFieldForInit, &nameParamRef);
    ExpressionStatement assignNameStmt({}, &assignName);
    std::vector<Statement*> initBody{ &assignNameStmt };
    MethodDeclareStatement initMethod({}, Token{ TokenType::IDENTIFIER, "init", 0 },
        { Token{ TokenType::IDENTIFIER, "name", 0 } }, initBody);

    ThisExpression thisForGetName(std::vector<Token>{});
    FieldAccessExpression nameFieldForGetName({}, &thisForGetName, Token{ TokenType::IDENTIFIER, "name", 0 });
    ReturnStatement returnName({}, &nameFieldForGetName);
    std::vector<Statement*> getNameBody{ &returnName };
    MethodDeclareStatement getNameMethod({}, Token{ TokenType::IDENTIFIER, "getName", 0 }, {}, getNameBody);

    std::vector<MethodDeclareStatement*> methods{ &initMethod, &getNameMethod };
    ClassDeclareStatement robotClass({}, Token{ TokenType::IDENTIFIER, "Robot", 0 }, methods);

    executor.execute(&robotClass);

    IdentifierExpression robotCallee({}, "Robot");
    StringExpression abc({}, "ABC");
    std::vector<Expression*> ctorArgs{ &abc };
    CallExpression construct({}, &robotCallee, ctorArgs);
    IdentifierExpression rIdent({}, "r");
    DeclareStatement declareR({}, &rIdent, &construct);
    executor.execute(&declareR);

    IdentifierExpression rRef({}, "r");
    FieldAccessExpression getNameAccess({}, &rRef, Token{ TokenType::IDENTIFIER, "getName", 0 });
    std::vector<Expression*> noArgs;
    CallExpression callGetName({}, &getNameAccess, noArgs);
    PrintStatement printResult({}, &callGetName);

    executor.execute(&printResult);

    EXPECT_EQ(out.str(), "ABC\n");
}

TEST(ExecutorClassTest, FieldAccess_ReadsDirectlyWithoutMethod) {
    // Class Point { init(x) { This.x = x; } }
    // var p = Point(3);
    // print p.x; // expect: 3
    std::ostringstream out;
    Executor executor(out);

    ThisExpression thisExpr(std::vector<Token>{});
    IdentifierExpression xParamRef({}, "x");
    FieldAccessExpression xField({}, &thisExpr, Token{ TokenType::IDENTIFIER, "x", 0 });
    AssignExpression assignX({}, &xField, &xParamRef);
    ExpressionStatement assignXStmt({}, &assignX);
    std::vector<Statement*> initBody{ &assignXStmt };
    MethodDeclareStatement initMethod({}, Token{ TokenType::IDENTIFIER, "init", 0 },
        { Token{ TokenType::IDENTIFIER, "x", 0 } }, initBody);
    std::vector<MethodDeclareStatement*> methods{ &initMethod };
    ClassDeclareStatement pointClass({}, Token{ TokenType::IDENTIFIER, "Point", 0 }, methods);

    executor.execute(&pointClass);

    IdentifierExpression pointCallee({}, "Point");
    NumberExpression three({}, 3);
    std::vector<Expression*> ctorArgs{ &three };
    CallExpression construct({}, &pointCallee, ctorArgs);
    IdentifierExpression pIdent({}, "p");
    DeclareStatement declareP({}, &pIdent, &construct);
    executor.execute(&declareP);

    IdentifierExpression pRef({}, "p");
    FieldAccessExpression readX({}, &pRef, Token{ TokenType::IDENTIFIER, "x", 0 });
    PrintStatement printResult({}, &readX);

    executor.execute(&printResult);

    EXPECT_EQ(out.str(), "3\n");
}

TEST(ExecutorClassTest, AccessingNonexistentField_ThrowsExecutorError) {
    std::ostringstream out;
    Executor executor(out);

    std::vector<MethodDeclareStatement*> methods;
    ClassDeclareStatement emptyClass({}, Token{ TokenType::IDENTIFIER, "Empty", 0 }, methods);
    executor.execute(&emptyClass);

    IdentifierExpression calleeRef({}, "Empty");
    std::vector<Expression*> noArgs;
    CallExpression construct({}, &calleeRef, noArgs);
    IdentifierExpression eIdent({}, "e");
    DeclareStatement declareE({}, &eIdent, &construct);
    executor.execute(&declareE);

    IdentifierExpression eRef({}, "e");
    FieldAccessExpression missingField({}, &eRef, Token{ TokenType::IDENTIFIER, "missing", 0 });

    EXPECT_THROW(executor.evaluate(&missingField), ExecutorError);
}

TEST(ExecutorClassTest, CallingNonexistentMethod_ThrowsExecutorError) {
    std::ostringstream out;
    Executor executor(out);

    std::vector<MethodDeclareStatement*> methods;
    ClassDeclareStatement emptyClass({}, Token{ TokenType::IDENTIFIER, "Empty", 0 }, methods);
    executor.execute(&emptyClass);

    IdentifierExpression calleeRef({}, "Empty");
    std::vector<Expression*> noCtorArgs;
    CallExpression construct({}, &calleeRef, noCtorArgs);
    IdentifierExpression eIdent({}, "e");
    DeclareStatement declareE({}, &eIdent, &construct);
    executor.execute(&declareE);

    IdentifierExpression eRef({}, "e");
    FieldAccessExpression missingMethod({}, &eRef, Token{ TokenType::IDENTIFIER, "missing", 0 });
    std::vector<Expression*> noArgs;
    CallExpression callMissing({}, &missingMethod, noArgs);

    EXPECT_THROW(executor.evaluate(&callMissing), ExecutorError);
}

TEST(ExecutorClassTest, ThisOutsideMethod_ThrowsExecutorError) {
    std::ostringstream out;
    Executor executor(out);

    ThisExpression thisExpr(std::vector<Token>{});

    EXPECT_THROW(executor.evaluate(&thisExpr), ExecutorError);
}

TEST(ExecutorClassTest, InstanceOf_SameClass_ReturnsTrue) {
    std::ostringstream out;
    Executor executor(out);

    std::vector<MethodDeclareStatement*> methods;
    ClassDeclareStatement robotClass({}, Token{ TokenType::IDENTIFIER, "Robot", 0 }, methods);
    executor.execute(&robotClass);

    IdentifierExpression calleeRef({}, "Robot");
    std::vector<Expression*> noArgs;
    CallExpression construct({}, &calleeRef, noArgs);
    IdentifierExpression rIdent({}, "r");
    DeclareStatement declareR({}, &rIdent, &construct);
    executor.execute(&declareR);

    IdentifierExpression rRef({}, "r");
    InstanceOfExpression instOf({}, &rRef, Token{ TokenType::IDENTIFIER, "Robot", 0 });

    EXPECT_TRUE(executor.evaluate(&instOf).asBoolean());
}

TEST(ExecutorClassTest, InstanceOf_DifferentClass_ReturnsFalse) {
    std::ostringstream out;
    Executor executor(out);

    std::vector<MethodDeclareStatement*> robotMethods;
    ClassDeclareStatement robotClass({}, Token{ TokenType::IDENTIFIER, "Robot", 0 }, robotMethods);
    executor.execute(&robotClass);

    std::vector<MethodDeclareStatement*> catMethods;
    ClassDeclareStatement catClass({}, Token{ TokenType::IDENTIFIER, "Cat", 0 }, catMethods);
    executor.execute(&catClass);

    IdentifierExpression calleeRef({}, "Robot");
    std::vector<Expression*> noArgs;
    CallExpression construct({}, &calleeRef, noArgs);
    IdentifierExpression rIdent({}, "r");
    DeclareStatement declareR({}, &rIdent, &construct);
    executor.execute(&declareR);

    IdentifierExpression rRef({}, "r");
    InstanceOfExpression instOf({}, &rRef, Token{ TokenType::IDENTIFIER, "Cat", 0 });

    EXPECT_FALSE(executor.evaluate(&instOf).asBoolean());
}

TEST(ExecutorClassTest, WritingUndeclaredField_AddsFieldToInstance) {
    // Class Empty {}
    // var e = Empty();
    // e.value = 42;  // init에서 선언된 적 없는 필드 - 외부에서 처음 write.
    // print e.value; // expect: 42
    std::ostringstream out;
    Executor executor(out);

    std::vector<MethodDeclareStatement*> methods;
    ClassDeclareStatement emptyClass({}, Token{ TokenType::IDENTIFIER, "Empty", 0 }, methods);
    executor.execute(&emptyClass);

    IdentifierExpression calleeRef({}, "Empty");
    std::vector<Expression*> noArgs;
    CallExpression construct({}, &calleeRef, noArgs);
    IdentifierExpression eIdent({}, "e");
    DeclareStatement declareE({}, &eIdent, &construct);
    executor.execute(&declareE);

    IdentifierExpression eRefForWrite({}, "e");
    FieldAccessExpression valueFieldForWrite({}, &eRefForWrite, Token{ TokenType::IDENTIFIER, "value", 0 });
    NumberExpression fortyTwo({}, 42);
    AssignExpression assignValue({}, &valueFieldForWrite, &fortyTwo);
    ExpressionStatement assignValueStmt({}, &assignValue);
    executor.execute(&assignValueStmt);

    IdentifierExpression eRefForRead({}, "e");
    FieldAccessExpression valueFieldForRead({}, &eRefForRead, Token{ TokenType::IDENTIFIER, "value", 0 });
    PrintStatement printResult({}, &valueFieldForRead);

    executor.execute(&printResult);

    EXPECT_EQ(out.str(), "42\n");
}

TEST(ExecutorClassTest, InstanceOf_NonInstanceOperand_ReturnsFalse) {
    std::ostringstream out;
    Executor executor(out);

    std::vector<MethodDeclareStatement*> methods;
    ClassDeclareStatement robotClass({}, Token{ TokenType::IDENTIFIER, "Robot", 0 }, methods);
    executor.execute(&robotClass);

    NumberExpression notAnInstance({}, 1);
    InstanceOfExpression instOf({}, &notAnInstance, Token{ TokenType::IDENTIFIER, "Robot", 0 });

    EXPECT_FALSE(executor.evaluate(&instOf).asBoolean());
}

// 클래스 상속(Super, `:`) - findMethod/resolveSuperclass의 superclass 체인 탐색.

TEST(ExecutorClassTest, MethodOverriding_ChildImplementationWins) {
    // Class Robot { move(dist) { print "move"; } }
    // Class SpeedRobot : Robot { move(dist) { print "speed move"; } }
    // SpeedRobot().move(3); // expect: speed move
    std::ostringstream out;
    Executor executor(out);

    StringExpression moveMsg({}, "move");
    PrintStatement printMove({}, &moveMsg);
    std::vector<Statement*> robotMoveBody{ &printMove };
    MethodDeclareStatement robotMove({}, Token{ TokenType::IDENTIFIER, "move", 0 },
        { Token{ TokenType::IDENTIFIER, "dist", 0 } }, robotMoveBody);
    std::vector<MethodDeclareStatement*> robotMethods{ &robotMove };
    ClassDeclareStatement robotClass({}, Token{ TokenType::IDENTIFIER, "Robot", 0 }, robotMethods);
    executor.execute(&robotClass);

    StringExpression speedMoveMsg({}, "speed move");
    PrintStatement printSpeedMove({}, &speedMoveMsg);
    std::vector<Statement*> speedMoveBody{ &printSpeedMove };
    MethodDeclareStatement speedMove({}, Token{ TokenType::IDENTIFIER, "move", 0 },
        { Token{ TokenType::IDENTIFIER, "dist", 0 } }, speedMoveBody);
    std::vector<MethodDeclareStatement*> speedMethods{ &speedMove };
    IdentifierExpression superclassRef({}, "Robot");
    ClassDeclareStatement speedRobotClass({}, Token{ TokenType::IDENTIFIER, "SpeedRobot", 0 }, speedMethods, &superclassRef);
    executor.execute(&speedRobotClass);

    IdentifierExpression calleeRef({}, "SpeedRobot");
    std::vector<Expression*> noArgs;
    CallExpression construct({}, &calleeRef, noArgs);
    IdentifierExpression rIdent({}, "r");
    DeclareStatement declareR({}, &rIdent, &construct);
    executor.execute(&declareR);

    IdentifierExpression rRef({}, "r");
    FieldAccessExpression moveAccess({}, &rRef, Token{ TokenType::IDENTIFIER, "move", 0 });
    NumberExpression three({}, 3);
    std::vector<Expression*> callArgs{ &three };
    CallExpression callMove({}, &moveAccess, callArgs);
    ExpressionStatement callMoveStmt({}, &callMove);

    executor.execute(&callMoveStmt);

    EXPECT_EQ(out.str(), "speed move\n");
}

TEST(ExecutorClassTest, NonOverriddenMethod_InheritsParentImplementation) {
    // Class Robot { move(dist) { print "move"; } }
    // Class SpeedRobot : Robot { }
    // SpeedRobot().move(3); // expect: move (부모 구현 그대로 상속)
    std::ostringstream out;
    Executor executor(out);

    StringExpression moveMsg({}, "move");
    PrintStatement printMove({}, &moveMsg);
    std::vector<Statement*> robotMoveBody{ &printMove };
    MethodDeclareStatement robotMove({}, Token{ TokenType::IDENTIFIER, "move", 0 },
        { Token{ TokenType::IDENTIFIER, "dist", 0 } }, robotMoveBody);
    std::vector<MethodDeclareStatement*> robotMethods{ &robotMove };
    ClassDeclareStatement robotClass({}, Token{ TokenType::IDENTIFIER, "Robot", 0 }, robotMethods);
    executor.execute(&robotClass);

    std::vector<MethodDeclareStatement*> speedMethods;  // 재정의 없음.
    IdentifierExpression superclassRef({}, "Robot");
    ClassDeclareStatement speedRobotClass({}, Token{ TokenType::IDENTIFIER, "SpeedRobot", 0 }, speedMethods, &superclassRef);
    executor.execute(&speedRobotClass);

    IdentifierExpression calleeRef({}, "SpeedRobot");
    std::vector<Expression*> noArgs;
    CallExpression construct({}, &calleeRef, noArgs);
    IdentifierExpression rIdent({}, "r");
    DeclareStatement declareR({}, &rIdent, &construct);
    executor.execute(&declareR);

    IdentifierExpression rRef({}, "r");
    FieldAccessExpression moveAccess({}, &rRef, Token{ TokenType::IDENTIFIER, "move", 0 });
    NumberExpression three({}, 3);
    std::vector<Expression*> callArgs{ &three };
    CallExpression callMove({}, &moveAccess, callArgs);
    ExpressionStatement callMoveStmt({}, &callMove);

    executor.execute(&callMoveStmt);

    EXPECT_EQ(out.str(), "move\n");
}

TEST(ExecutorClassTest, SuperCall_InvokesParentImplementationBeforeChildContinues) {
    // Class Robot { move(dist) { print "move"; } }
    // Class SpeedRobot : Robot { move(dist) { Super.move(dist); print "speed!"; } }
    // SpeedRobot().move(3); // expect: move, speed! (이 순서로)
    std::ostringstream out;
    Executor executor(out);

    StringExpression moveMsg({}, "move");
    PrintStatement printMove({}, &moveMsg);
    std::vector<Statement*> robotMoveBody{ &printMove };
    MethodDeclareStatement robotMove({}, Token{ TokenType::IDENTIFIER, "move", 0 },
        { Token{ TokenType::IDENTIFIER, "dist", 0 } }, robotMoveBody);
    std::vector<MethodDeclareStatement*> robotMethods{ &robotMove };
    ClassDeclareStatement robotClass({}, Token{ TokenType::IDENTIFIER, "Robot", 0 }, robotMethods);
    executor.execute(&robotClass);

    SuperExpression superExpr(std::vector<Token>{});
    FieldAccessExpression superMoveAccess({}, &superExpr, Token{ TokenType::IDENTIFIER, "move", 0 });
    IdentifierExpression distRef({}, "dist");
    std::vector<Expression*> superCallArgs{ &distRef };
    CallExpression callSuperMove({}, &superMoveAccess, superCallArgs);
    ExpressionStatement callSuperMoveStmt({}, &callSuperMove);
    StringExpression speedBangMsg({}, "speed!");
    PrintStatement printSpeedBang({}, &speedBangMsg);
    std::vector<Statement*> speedMoveBody{ &callSuperMoveStmt, &printSpeedBang };
    MethodDeclareStatement speedMove({}, Token{ TokenType::IDENTIFIER, "move", 0 },
        { Token{ TokenType::IDENTIFIER, "dist", 0 } }, speedMoveBody);
    std::vector<MethodDeclareStatement*> speedMethods{ &speedMove };
    IdentifierExpression superclassRef({}, "Robot");
    ClassDeclareStatement speedRobotClass({}, Token{ TokenType::IDENTIFIER, "SpeedRobot", 0 }, speedMethods, &superclassRef);
    executor.execute(&speedRobotClass);

    IdentifierExpression calleeRef({}, "SpeedRobot");
    std::vector<Expression*> noArgs;
    CallExpression construct({}, &calleeRef, noArgs);
    IdentifierExpression rIdent({}, "r");
    DeclareStatement declareR({}, &rIdent, &construct);
    executor.execute(&declareR);

    IdentifierExpression rRef({}, "r");
    FieldAccessExpression moveAccess({}, &rRef, Token{ TokenType::IDENTIFIER, "move", 0 });
    NumberExpression three({}, 3);
    std::vector<Expression*> callArgs{ &three };
    CallExpression callMove({}, &moveAccess, callArgs);
    ExpressionStatement callMoveStmt({}, &callMove);

    executor.execute(&callMoveStmt);

    EXPECT_EQ(out.str(), "move\nspeed!\n");
}

TEST(ExecutorClassTest, SuperCall_SharesSameInstanceFieldsAsCaller) {
    // Class Robot { move(dist) { This.position = This.position + dist; } }
    // Class SpeedRobot : Robot { move(dist) { Super.move(dist); } }
    // var r = SpeedRobot(); r.position = 0; r.move(5); print r.position; // expect: 5
    std::ostringstream out;
    Executor executor(out);

    ThisExpression thisWrite(std::vector<Token>{});
    FieldAccessExpression posWriteTarget({}, &thisWrite, Token{ TokenType::IDENTIFIER, "position", 0 });
    ThisExpression thisRead(std::vector<Token>{});
    FieldAccessExpression posReadForAdd({}, &thisRead, Token{ TokenType::IDENTIFIER, "position", 0 });
    IdentifierExpression distRefForAdd({}, "dist");
    AddExpression sumExpr({}, &posReadForAdd, &distRefForAdd);
    AssignExpression assignPos({}, &posWriteTarget, &sumExpr);
    ExpressionStatement assignPosStmt({}, &assignPos);
    std::vector<Statement*> robotMoveBody{ &assignPosStmt };
    MethodDeclareStatement robotMove({}, Token{ TokenType::IDENTIFIER, "move", 0 },
        { Token{ TokenType::IDENTIFIER, "dist", 0 } }, robotMoveBody);
    std::vector<MethodDeclareStatement*> robotMethods{ &robotMove };
    ClassDeclareStatement robotClass({}, Token{ TokenType::IDENTIFIER, "Robot", 0 }, robotMethods);
    executor.execute(&robotClass);

    SuperExpression superExpr(std::vector<Token>{});
    FieldAccessExpression superMoveAccess({}, &superExpr, Token{ TokenType::IDENTIFIER, "move", 0 });
    IdentifierExpression distRefForSuperCall({}, "dist");
    std::vector<Expression*> superCallArgs{ &distRefForSuperCall };
    CallExpression callSuperMove({}, &superMoveAccess, superCallArgs);
    ExpressionStatement callSuperMoveStmt({}, &callSuperMove);
    std::vector<Statement*> speedMoveBody{ &callSuperMoveStmt };
    MethodDeclareStatement speedMove({}, Token{ TokenType::IDENTIFIER, "move", 0 },
        { Token{ TokenType::IDENTIFIER, "dist", 0 } }, speedMoveBody);
    std::vector<MethodDeclareStatement*> speedMethods{ &speedMove };
    IdentifierExpression superclassRef({}, "Robot");
    ClassDeclareStatement speedRobotClass({}, Token{ TokenType::IDENTIFIER, "SpeedRobot", 0 }, speedMethods, &superclassRef);
    executor.execute(&speedRobotClass);

    IdentifierExpression calleeRef({}, "SpeedRobot");
    std::vector<Expression*> noArgs;
    CallExpression construct({}, &calleeRef, noArgs);
    IdentifierExpression rIdent({}, "r");
    DeclareStatement declareR({}, &rIdent, &construct);
    executor.execute(&declareR);

    IdentifierExpression rRefForInit({}, "r");
    FieldAccessExpression posInitTarget({}, &rRefForInit, Token{ TokenType::IDENTIFIER, "position", 0 });
    NumberExpression zero({}, 0);
    AssignExpression assignZero({}, &posInitTarget, &zero);
    ExpressionStatement assignZeroStmt({}, &assignZero);
    executor.execute(&assignZeroStmt);

    IdentifierExpression rRefForMove({}, "r");
    FieldAccessExpression moveAccess({}, &rRefForMove, Token{ TokenType::IDENTIFIER, "move", 0 });
    NumberExpression five({}, 5);
    std::vector<Expression*> callArgs{ &five };
    CallExpression callMove({}, &moveAccess, callArgs);
    ExpressionStatement callMoveStmt({}, &callMove);
    executor.execute(&callMoveStmt);

    IdentifierExpression rRefForPrint({}, "r");
    FieldAccessExpression posRead({}, &rRefForPrint, Token{ TokenType::IDENTIFIER, "position", 0 });
    PrintStatement printResult({}, &posRead);
    executor.execute(&printResult);

    EXPECT_EQ(out.str(), "5\n");
}

TEST(ExecutorClassTest, MultilevelInheritance_GrandparentMethodIsReachable) {
    // Class A { greet() { print "A"; } }
    // Class B : A { }
    // Class C : B { }
    // C().greet(); // expect: A (조부모 메서드까지 체인을 타고 올라가 탐색)
    std::ostringstream out;
    Executor executor(out);

    StringExpression aMsg({}, "A");
    PrintStatement printA({}, &aMsg);
    std::vector<Statement*> greetBody{ &printA };
    MethodDeclareStatement greetMethod({}, Token{ TokenType::IDENTIFIER, "greet", 0 }, {}, greetBody);
    std::vector<MethodDeclareStatement*> aMethods{ &greetMethod };
    ClassDeclareStatement classA({}, Token{ TokenType::IDENTIFIER, "A", 0 }, aMethods);
    executor.execute(&classA);

    std::vector<MethodDeclareStatement*> bMethods;
    IdentifierExpression superRefB({}, "A");
    ClassDeclareStatement classB({}, Token{ TokenType::IDENTIFIER, "B", 0 }, bMethods, &superRefB);
    executor.execute(&classB);

    std::vector<MethodDeclareStatement*> cMethods;
    IdentifierExpression superRefC({}, "B");
    ClassDeclareStatement classC({}, Token{ TokenType::IDENTIFIER, "C", 0 }, cMethods, &superRefC);
    executor.execute(&classC);

    IdentifierExpression calleeRef({}, "C");
    std::vector<Expression*> noCtorArgs;
    CallExpression construct({}, &calleeRef, noCtorArgs);
    IdentifierExpression cIdent({}, "c");
    DeclareStatement declareC({}, &cIdent, &construct);
    executor.execute(&declareC);

    IdentifierExpression cRef({}, "c");
    FieldAccessExpression greetAccess({}, &cRef, Token{ TokenType::IDENTIFIER, "greet", 0 });
    std::vector<Expression*> noArgs;
    CallExpression callGreet({}, &greetAccess, noArgs);
    ExpressionStatement callGreetStmt({}, &callGreet);

    executor.execute(&callGreetStmt);

    EXPECT_EQ(out.str(), "A\n");
}

TEST(ExecutorClassTest, InstanceOf_MatchesAnyClassInSuperclassChainButNotUnrelatedClass) {
    // Class Robot { } Class SpeedRobot : Robot { } Class Other { }
    // var w = SpeedRobot();
    // w instanceof SpeedRobot -> true, w instanceof Robot -> true, w instanceof Other -> false
    std::ostringstream out;
    Executor executor(out);

    std::vector<MethodDeclareStatement*> robotMethods;
    ClassDeclareStatement robotClass({}, Token{ TokenType::IDENTIFIER, "Robot", 0 }, robotMethods);
    executor.execute(&robotClass);

    std::vector<MethodDeclareStatement*> speedMethods;
    IdentifierExpression superclassRef({}, "Robot");
    ClassDeclareStatement speedRobotClass({}, Token{ TokenType::IDENTIFIER, "SpeedRobot", 0 }, speedMethods, &superclassRef);
    executor.execute(&speedRobotClass);

    std::vector<MethodDeclareStatement*> otherMethods;
    ClassDeclareStatement otherClass({}, Token{ TokenType::IDENTIFIER, "Other", 0 }, otherMethods);
    executor.execute(&otherClass);

    IdentifierExpression calleeRef({}, "SpeedRobot");
    std::vector<Expression*> noArgs;
    CallExpression construct({}, &calleeRef, noArgs);
    IdentifierExpression wIdent({}, "w");
    DeclareStatement declareW({}, &wIdent, &construct);
    executor.execute(&declareW);

    IdentifierExpression wRefForSpeed({}, "w");
    InstanceOfExpression instOfSpeed({}, &wRefForSpeed, Token{ TokenType::IDENTIFIER, "SpeedRobot", 0 });
    EXPECT_TRUE(executor.evaluate(&instOfSpeed).asBoolean());

    IdentifierExpression wRefForRobot({}, "w");
    InstanceOfExpression instOfRobot({}, &wRefForRobot, Token{ TokenType::IDENTIFIER, "Robot", 0 });
    EXPECT_TRUE(executor.evaluate(&instOfRobot).asBoolean());

    IdentifierExpression wRefForOther({}, "w");
    InstanceOfExpression instOfOther({}, &wRefForOther, Token{ TokenType::IDENTIFIER, "Other", 0 });
    EXPECT_FALSE(executor.evaluate(&instOfOther).asBoolean());
}

TEST(ExecutorClassTest, SuperCall_WithoutSuperclass_ThrowsExecutorError) {
    // Class Robot { move(dist) { Super.move(dist); } } // 부모 없음.
    // Robot().move(1); // expect: ExecutorError
    std::ostringstream out;
    Executor executor(out);

    SuperExpression superExpr(std::vector<Token>{});
    FieldAccessExpression superMoveAccess({}, &superExpr, Token{ TokenType::IDENTIFIER, "move", 0 });
    IdentifierExpression distRef({}, "dist");
    std::vector<Expression*> superCallArgs{ &distRef };
    CallExpression callSuperMove({}, &superMoveAccess, superCallArgs);
    ExpressionStatement callSuperMoveStmt({}, &callSuperMove);
    std::vector<Statement*> moveBody{ &callSuperMoveStmt };
    MethodDeclareStatement moveMethod({}, Token{ TokenType::IDENTIFIER, "move", 0 },
        { Token{ TokenType::IDENTIFIER, "dist", 0 } }, moveBody);
    std::vector<MethodDeclareStatement*> methods{ &moveMethod };
    ClassDeclareStatement robotClass({}, Token{ TokenType::IDENTIFIER, "Robot", 0 }, methods);
    executor.execute(&robotClass);

    IdentifierExpression calleeRef({}, "Robot");
    std::vector<Expression*> noCtorArgs;
    CallExpression construct({}, &calleeRef, noCtorArgs);
    IdentifierExpression rIdent({}, "r");
    DeclareStatement declareR({}, &rIdent, &construct);
    executor.execute(&declareR);

    IdentifierExpression rRef({}, "r");
    FieldAccessExpression moveAccess({}, &rRef, Token{ TokenType::IDENTIFIER, "move", 0 });
    NumberExpression one({}, 1);
    std::vector<Expression*> callArgs{ &one };
    CallExpression callMove({}, &moveAccess, callArgs);

    EXPECT_THROW(executor.evaluate(&callMove), ExecutorError);
}
