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
