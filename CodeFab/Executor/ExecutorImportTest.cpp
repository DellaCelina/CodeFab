#include <sstream>
#include <vector>

#include "gmock/gmock.h"
#include "Executor.h"
#include "../Assembler/SyntaxTree.h"

// import 실행: alias 스코프에 선언들을 모아 등록하고, alias.name으로 접근한다.
// 실제 파일 읽기/재귀 컴파일은 Assembler 담당이라, 여기서는 이미 파싱된
// declarations를 직접 손으로 만들어 검증한다.

TEST(ExecutorImportTest, VariableDeclaration_AccessibleThroughAlias) {
    // import "math.cf" alias math;  // math.cf: var PI = 3;
    // print math.PI; // expect: 3
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression piIdent({}, "PI");
    NumberExpression three({}, 3);
    DeclareStatement declarePi({}, &piIdent, &three);
    std::vector<Statement*> declarations{ &declarePi };
    ImportStatement importStmt({}, Token{ TokenType::IDENTIFIER, "math", 0 }, declarations);

    executor.execute(&importStmt);

    IdentifierExpression mathRef({}, "math");
    FieldAccessExpression piAccess({}, &mathRef, Token{ TokenType::IDENTIFIER, "PI", 0 });
    PrintStatement printResult({}, &piAccess);

    executor.execute(&printResult);

    EXPECT_EQ(out.str(), "3\n");
}

TEST(ExecutorImportTest, FunctionDeclaration_CallableThroughAlias) {
    // import "math.cf" alias math;  // math.cf: Func square(x) { return x * x; }
    // print math.square(4); // expect: 16
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression xRef1({}, "x");
    IdentifierExpression xRef2({}, "x");
    MultExpression xTimesX({}, &xRef1, &xRef2);
    ReturnStatement returnSquare({}, &xTimesX);
    std::vector<Statement*> squareBody{ &returnSquare };
    FunctionDeclareStatement squareDecl({}, Token{ TokenType::IDENTIFIER, "square", 0 },
        { Token{ TokenType::IDENTIFIER, "x", 0 } }, squareBody);
    std::vector<Statement*> declarations{ &squareDecl };
    ImportStatement importStmt({}, Token{ TokenType::IDENTIFIER, "math", 0 }, declarations);

    executor.execute(&importStmt);

    IdentifierExpression mathRef({}, "math");
    FieldAccessExpression squareAccess({}, &mathRef, Token{ TokenType::IDENTIFIER, "square", 0 });
    NumberExpression four({}, 4);
    std::vector<Expression*> args{ &four };
    CallExpression callSquare({}, &squareAccess, args);
    PrintStatement printResult({}, &callSquare);

    executor.execute(&printResult);

    EXPECT_EQ(out.str(), "16\n");
}

TEST(ExecutorImportTest, DeclaredNames_DoNotLeakToGlobalScope) {
    // import "math.cf" alias math;  // math.cf: var PI = 3;
    // print PI; // expect: ExecutorError (정의되지 않은 변수)
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression piIdent({}, "PI");
    NumberExpression three({}, 3);
    DeclareStatement declarePi({}, &piIdent, &three);
    std::vector<Statement*> declarations{ &declarePi };
    ImportStatement importStmt({}, Token{ TokenType::IDENTIFIER, "math", 0 }, declarations);

    executor.execute(&importStmt);

    IdentifierExpression leakedRef({}, "PI");
    EXPECT_THROW(executor.evaluate(&leakedRef), ExecutorError);
}

TEST(ExecutorImportTest, ImportInsideFunctionBody_AccessibleWithinFunctionScope) {
    // Func useMath() {
    //   import "math.cf" alias math; // math.cf: var PI = 3;
    //   print math.PI;
    // }
    // useMath(); // expect: 3
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression piIdent({}, "PI");
    NumberExpression three({}, 3);
    DeclareStatement declarePi({}, &piIdent, &three);
    std::vector<Statement*> declarations{ &declarePi };
    ImportStatement importStmt({}, Token{ TokenType::IDENTIFIER, "math", 0 }, declarations);

    IdentifierExpression mathRef({}, "math");
    FieldAccessExpression piAccess({}, &mathRef, Token{ TokenType::IDENTIFIER, "PI", 0 });
    PrintStatement printResult({}, &piAccess);

    std::vector<Statement*> funcBody{ &importStmt, &printResult };
    FunctionDeclareStatement useMathDecl({}, Token{ TokenType::IDENTIFIER, "useMath", 0 }, {}, funcBody);
    executor.execute(&useMathDecl);

    IdentifierExpression calleeRef({}, "useMath");
    std::vector<Expression*> noArgs;
    CallExpression callUseMath({}, &calleeRef, noArgs);
    ExpressionStatement callStmt({}, &callUseMath);

    executor.execute(&callStmt);

    EXPECT_EQ(out.str(), "3\n");
}

TEST(ExecutorImportTest, ImportInsideFunctionBody_AliasDoesNotLeakOutside) {
    // Func useMath() { import "math.cf" alias math; }
    // useMath();
    // print math.PI; // expect: ExecutorError (함수 스코프 밖에서는 math를 모른다)
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression piIdent({}, "PI");
    NumberExpression three({}, 3);
    DeclareStatement declarePi({}, &piIdent, &three);
    std::vector<Statement*> declarations{ &declarePi };
    ImportStatement importStmt({}, Token{ TokenType::IDENTIFIER, "math", 0 }, declarations);

    std::vector<Statement*> funcBody{ &importStmt };
    FunctionDeclareStatement useMathDecl({}, Token{ TokenType::IDENTIFIER, "useMath", 0 }, {}, funcBody);
    executor.execute(&useMathDecl);

    IdentifierExpression calleeRef({}, "useMath");
    std::vector<Expression*> noArgs;
    CallExpression callUseMath({}, &calleeRef, noArgs);
    ExpressionStatement callStmt({}, &callUseMath);
    executor.execute(&callStmt);

    IdentifierExpression leakedRef({}, "math");
    EXPECT_THROW(executor.evaluate(&leakedRef), ExecutorError);
}

TEST(ExecutorImportTest, ImportInsideBlock_AccessibleWithinBlockScope) {
    // { import "math.cf" alias math; print math.PI; } // expect: 3
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression piIdent({}, "PI");
    NumberExpression three({}, 3);
    DeclareStatement declarePi({}, &piIdent, &three);
    std::vector<Statement*> declarations{ &declarePi };
    ImportStatement importStmt({}, Token{ TokenType::IDENTIFIER, "math", 0 }, declarations);

    IdentifierExpression mathRef({}, "math");
    FieldAccessExpression piAccess({}, &mathRef, Token{ TokenType::IDENTIFIER, "PI", 0 });
    PrintStatement printResult({}, &piAccess);

    std::vector<Statement*> blockStatements{ &importStmt, &printResult };
    BlockStatement block({}, blockStatements);

    executor.execute(&block);

    EXPECT_EQ(out.str(), "3\n");
}

TEST(ExecutorImportTest, ImportInsideBlock_AliasDoesNotLeakOutside) {
    // { import "math.cf" alias math; }
    // print math.PI; // expect: ExecutorError (블록 스코프 밖에서는 math를 모른다)
    std::ostringstream out;
    Executor executor(out);

    IdentifierExpression piIdent({}, "PI");
    NumberExpression three({}, 3);
    DeclareStatement declarePi({}, &piIdent, &three);
    std::vector<Statement*> declarations{ &declarePi };
    ImportStatement importStmt({}, Token{ TokenType::IDENTIFIER, "math", 0 }, declarations);

    std::vector<Statement*> blockStatements{ &importStmt };
    BlockStatement block({}, blockStatements);
    executor.execute(&block);

    IdentifierExpression leakedRef({}, "math");
    EXPECT_THROW(executor.evaluate(&leakedRef), ExecutorError);
}

TEST(ExecutorImportTest, AccessingMissingMember_ThrowsExecutorError) {
    std::ostringstream out;
    Executor executor(out);

    std::vector<Statement*> declarations;
    ImportStatement importStmt({}, Token{ TokenType::IDENTIFIER, "math", 0 }, declarations);
    executor.execute(&importStmt);

    IdentifierExpression mathRef({}, "math");
    FieldAccessExpression missing({}, &mathRef, Token{ TokenType::IDENTIFIER, "missing", 0 });

    EXPECT_THROW(executor.evaluate(&missing), ExecutorError);
}

TEST(ExecutorImportTest, CallingMissingFunction_ThrowsExecutorError) {
    std::ostringstream out;
    Executor executor(out);

    std::vector<Statement*> declarations;
    ImportStatement importStmt({}, Token{ TokenType::IDENTIFIER, "math", 0 }, declarations);
    executor.execute(&importStmt);

    IdentifierExpression mathRef({}, "math");
    FieldAccessExpression missing({}, &mathRef, Token{ TokenType::IDENTIFIER, "missing", 0 });
    std::vector<Expression*> noArgs;
    CallExpression callMissing({}, &missing, noArgs);

    EXPECT_THROW(executor.evaluate(&callMissing), ExecutorError);
}
