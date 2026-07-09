#include <sstream>

#include "gmock/gmock.h"
#include "Checker.h"
#include "../Executor/Executor.h"

// tokenizer/assembler 없이 checker만 독립적으로 검증하기 위해 트리를 직접 구성한다.

// 대표 토큰의 type/origin이 검사에 의미가 없는 구조적 노드(BlockStatement 등)용 더미.
static std::vector<Token> testTokens(int line) {
    return { Token{ TokenType::NUMBER, "", line } };
}

// 실제 토큰(리터럴/연산자/키워드/식별자)에 정확히 대응시키고 싶을 때 사용.
static std::vector<Token> testTokens(TokenType type, const std::string& origin, int line) {
    return { Token{ type, origin, line } };
}

// 대부분의 테스트가 공유하는 준비물: 빈 트리 하나, 실제 Executor, 그 위의 Checker.
class CheckerTest : public ::testing::Test {
protected:
    SyntaxTree tree;
    std::ostringstream executorOutput;
    Executor executor{ executorOutput };
    Checker checker{ executor };
};

// print 1 + 2 * 3;   // expect: 7
TEST_F(CheckerTest, PrintExpressionWithNoErrorsPasses) {
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto lit2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "2", 1), 2.0);
    auto lit3 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "3", 1), 3.0);

    auto mult = std::make_unique<MultExpression>(testTokens(TokenType::STAR, "*", 1), lit2.get(), lit3.get());
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 1), lit1.get(), mult.get());
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), add.get());
    auto block = std::make_unique<BlockStatement>(testTokens(1), std::vector<Statement*>{ printStmt.get() });

    SyntaxNode* rootRaw = block.get();

    // SyntaxTree가 소유권을 갖도록 전부 넘긴다 (순서 무관, 소멸 시 한꺼번에 해제).
    tree.add(std::move(lit1));
    tree.add(std::move(lit2));
    tree.add(std::move(lit3));
    tree.add(std::move(mult));
    tree.add(std::move(add));
    tree.add(std::move(printStmt));
    tree.add(std::move(block));
    tree.setRoot(rootRaw);

    EXPECT_TRUE(checker.check(tree));
}

// { var a = 10; var a = 12; }   // 2번째 var a 에서 중복 선언 에러 (p.72)
TEST_F(CheckerTest, DuplicateDeclarationInSameScopeReportsError) {
    // DeclareStatement는 이름을 string이 아니라 IdentifierExpression*로 받으므로
    // "선언 대상 a"를 나타내는 식별자 노드를 별도로 만들어야 한다.
    auto declIdent1 = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 2), "a");
    auto lit10 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "10", 2), 10.0);
    auto declA1 = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 2), declIdent1.get(), lit10.get());

    auto declIdent2 = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 3), "a");
    auto lit12 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "12", 3), 12.0);
    auto declA2 = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 3), declIdent2.get(), lit12.get());

    auto block = std::make_unique<BlockStatement>(testTokens(1), std::vector<Statement*>{ declA1.get(), declA2.get() });

    SyntaxNode* rootRaw = block.get();

    tree.add(std::move(declIdent1));
    tree.add(std::move(lit10));
    tree.add(std::move(declA1));
    tree.add(std::move(declIdent2));
    tree.add(std::move(lit12));
    tree.add(std::move(declA2));
    tree.add(std::move(block));
    tree.setRoot(rootRaw);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        // CheckerError는 줄 번호를 따로 들고 있지 않고 메시지에 직접 담는다
        // (CheckerInterface.h/checker.cpp의 reportError() 참고).
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("3번째 줄"));
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("이미 해당 변수는 현재 스코프에서 사용중입니다"));
    }
}

// { var a = a + 1; }   // 자신의 초기화식에서 자기 자신을 참조 -> 에러 (p.73)
TEST_F(CheckerTest, SelfReferenceInInitializerReportsError) {
    // 선언 대상(declIdent)과 초기화식 안에서 참조하는 식별자(idA)는 이름은 같지만
    // 서로 다른 노드 인스턴스다 (실제 파서도 이렇게 별도 토큰/노드로 만든다).
    auto declIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 2), "a");
    auto idA = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 2), "a");
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 2), 1.0);
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 2), idA.get(), lit1.get());
    auto declA = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 2), declIdent.get(), add.get());
    auto block = std::make_unique<BlockStatement>(testTokens(1), std::vector<Statement*>{ declA.get() });

    SyntaxNode* rootRaw = block.get();

    tree.add(std::move(declIdent));
    tree.add(std::move(idA));
    tree.add(std::move(lit1));
    tree.add(std::move(add));
    tree.add(std::move(declA));
    tree.add(std::move(block));
    tree.setRoot(rootRaw);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("2번째 줄"));
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("자신의 초기화식에서 지역변수를 읽을 수 없습니다"));
    }
}

// var a = 10;   (1번째 check() 호출)
// print a;      (2번째 check() 호출, 같은 Checker 인스턴스)
// REPL 한 줄마다 check()가 새로 호출되지만 scopes는 세션 내내 유지되므로 통과해야 한다.
TEST_F(CheckerTest, VariableDeclaredInEarlierCallIsVisibleToLaterCall) {
    SyntaxTree declareTree;
    auto declIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 1), "a");
    auto lit10 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "10", 1), 10.0);
    auto declA = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 1), declIdent.get(), lit10.get());
    SyntaxNode* declareRoot = declA.get();
    declareTree.add(std::move(declIdent));
    declareTree.add(std::move(lit10));
    declareTree.add(std::move(declA));
    declareTree.setRoot(declareRoot);

    ASSERT_TRUE(checker.check(declareTree));

    SyntaxTree printTree;
    auto idA = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 2), "a");
    auto printA = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 2), idA.get());
    SyntaxNode* printRoot = printA.get();
    printTree.add(std::move(idA));
    printTree.add(std::move(printA));
    printTree.setRoot(printRoot);

    EXPECT_TRUE(checker.check(printTree));
}

// print notDefined;   (선언된 적 없는 변수 - 세션 전체에 걸쳐도 여전히 에러여야 한다)
TEST_F(CheckerTest, UndefinedVariableAcrossCallsStillReportsError) {
    auto idNotDefined = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "notDefined", 1), "notDefined");
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), idNotDefined.get());
    SyntaxNode* root = printStmt.get();
    tree.add(std::move(idNotDefined));
    tree.add(std::move(printStmt));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("'notDefined'에러: 선언되지 않은 변수입니다"));
    }
}

// if (true) { var a = 1; var a = 2; }
// checkStatement가 IfStatement의 thenBranch까지 재귀해서 내부 블록도 검사하는지 확인한다.
TEST_F(CheckerTest, DuplicateDeclarationInsideIfBlockReportsError) {
    auto cond = std::make_unique<BooleanExpression>(testTokens(TokenType::TRUE, "true", 1), true);

    auto declIdent1 = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 2), "a");
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 2), 1.0);
    auto declA1 = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 2), declIdent1.get(), lit1.get());

    auto declIdent2 = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 3), "a");
    auto lit2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "2", 3), 2.0);
    auto declA2 = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 3), declIdent2.get(), lit2.get());

    auto thenBlock = std::make_unique<BlockStatement>(testTokens(1), std::vector<Statement*>{ declA1.get(), declA2.get() });
    auto ifStmt = std::make_unique<IfStatement>(testTokens(TokenType::IF, "if", 1), cond.get(), thenBlock.get());

    SyntaxNode* rootRaw = ifStmt.get();

    tree.add(std::move(cond));
    tree.add(std::move(declIdent1));
    tree.add(std::move(lit1));
    tree.add(std::move(declA1));
    tree.add(std::move(declIdent2));
    tree.add(std::move(lit2));
    tree.add(std::move(declA2));
    tree.add(std::move(thenBlock));
    tree.add(std::move(ifStmt));
    tree.setRoot(rootRaw);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("3번째 줄"));
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("이미 해당 변수는 현재 스코프에서 사용중입니다"));
    }
}

// if (true) { var a = a + 1; }
// checkStatement가 IfStatement의 thenBranch 안 자기 참조도 잡아내는지 확인한다.
TEST_F(CheckerTest, SelfReferenceInsideIfBlockReportsError) {
    auto cond = std::make_unique<BooleanExpression>(testTokens(TokenType::TRUE, "true", 1), true);

    auto declIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 2), "a");
    auto idA = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 2), "a");
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 2), 1.0);
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 2), idA.get(), lit1.get());
    auto declA = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 2), declIdent.get(), add.get());

    auto thenBlock = std::make_unique<BlockStatement>(testTokens(1), std::vector<Statement*>{ declA.get() });
    auto ifStmt = std::make_unique<IfStatement>(testTokens(TokenType::IF, "if", 1), cond.get(), thenBlock.get());

    SyntaxNode* rootRaw = ifStmt.get();

    tree.add(std::move(cond));
    tree.add(std::move(declIdent));
    tree.add(std::move(idA));
    tree.add(std::move(lit1));
    tree.add(std::move(add));
    tree.add(std::move(declA));
    tree.add(std::move(thenBlock));
    tree.add(std::move(ifStmt));
    tree.setRoot(rootRaw);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("2번째 줄"));
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("자신의 초기화식에서 지역변수를 읽을 수 없습니다"));
    }
}

// for (var j = 0; true; 0) { var a = 1; var a = 2; }
// checkStatement가 ForStatement의 loop 본문까지 재귀해서 내부 블록도 검사하는지,
// 그리고 init에서 선언한 j가 for 전용 스코프에 들어가는지 함께 확인한다.
TEST_F(CheckerTest, DuplicateDeclarationInsideForBlockReportsError) {
    auto jIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "j", 1), "j");
    auto jInit = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "0", 1), 0.0);
    auto initDecl = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 1), jIdent.get(), jInit.get());
    auto compare = std::make_unique<BooleanExpression>(testTokens(TokenType::TRUE, "true", 1), true);
    auto next = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "0", 1), 0.0);

    auto declIdent1 = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 2), "a");
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 2), 1.0);
    auto declA1 = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 2), declIdent1.get(), lit1.get());

    auto declIdent2 = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 3), "a");
    auto lit2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "2", 3), 2.0);
    auto declA2 = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 3), declIdent2.get(), lit2.get());

    auto loopBlock = std::make_unique<BlockStatement>(testTokens(1), std::vector<Statement*>{ declA1.get(), declA2.get() });
    auto forStmt = std::make_unique<ForStatement>(testTokens(TokenType::FOR, "for", 1),
        initDecl.get(), compare.get(), next.get(), loopBlock.get());

    SyntaxNode* rootRaw = forStmt.get();

    tree.add(std::move(jIdent));
    tree.add(std::move(jInit));
    tree.add(std::move(initDecl));
    tree.add(std::move(compare));
    tree.add(std::move(next));
    tree.add(std::move(declIdent1));
    tree.add(std::move(lit1));
    tree.add(std::move(declA1));
    tree.add(std::move(declIdent2));
    tree.add(std::move(lit2));
    tree.add(std::move(declA2));
    tree.add(std::move(loopBlock));
    tree.add(std::move(forStmt));
    tree.setRoot(rootRaw);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("3번째 줄"));
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("이미 해당 변수는 현재 스코프에서 사용중입니다"));
    }
}

// for (var j = 0; true; 0) { var a = a + 1; }
// checkStatement가 ForStatement의 loop 본문 안 자기 참조도 잡아내는지 확인한다.
TEST_F(CheckerTest, SelfReferenceInsideForBlockReportsError) {
    auto jIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "j", 1), "j");
    auto jInit = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "0", 1), 0.0);
    auto initDecl = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 1), jIdent.get(), jInit.get());
    auto compare = std::make_unique<BooleanExpression>(testTokens(TokenType::TRUE, "true", 1), true);
    auto next = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "0", 1), 0.0);

    auto declIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 2), "a");
    auto idA = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 2), "a");
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 2), 1.0);
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 2), idA.get(), lit1.get());
    auto declA = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 2), declIdent.get(), add.get());

    auto loopBlock = std::make_unique<BlockStatement>(testTokens(1), std::vector<Statement*>{ declA.get() });
    auto forStmt = std::make_unique<ForStatement>(testTokens(TokenType::FOR, "for", 1),
        initDecl.get(), compare.get(), next.get(), loopBlock.get());

    SyntaxNode* rootRaw = forStmt.get();

    tree.add(std::move(jIdent));
    tree.add(std::move(jInit));
    tree.add(std::move(initDecl));
    tree.add(std::move(compare));
    tree.add(std::move(next));
    tree.add(std::move(declIdent));
    tree.add(std::move(idA));
    tree.add(std::move(lit1));
    tree.add(std::move(add));
    tree.add(std::move(declA));
    tree.add(std::move(loopBlock));
    tree.add(std::move(forStmt));
    tree.setRoot(rootRaw);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("2번째 줄"));
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("자신의 초기화식에서 지역변수를 읽을 수 없습니다"));
    }
}

// for (var j = 0; ...) { ... }  다음 for (var j = 0; ...) { ... }
// for의 init에서 선언한 루프 변수는 전용 스코프에 갇혀야 하므로, 같은 이름의 루프
// 변수를 쓰는 두 개의 for문이 연달아 있어도 "중복 선언" 오류가 나면 안 된다.
TEST_F(CheckerTest, ForLoopVariableDoesNotLeakIntoEnclosingScope) {
    auto jIdent1 = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "j", 1), "j");
    auto jInit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "0", 1), 0.0);
    auto initDecl1 = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 1), jIdent1.get(), jInit1.get());
    auto compare1 = std::make_unique<BooleanExpression>(testTokens(TokenType::TRUE, "true", 1), true);
    auto next1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "0", 1), 0.0);
    auto loopBlock1 = std::make_unique<BlockStatement>(testTokens(1), std::vector<Statement*>{});
    auto forStmt1 = std::make_unique<ForStatement>(testTokens(TokenType::FOR, "for", 1),
        initDecl1.get(), compare1.get(), next1.get(), loopBlock1.get());

    auto jIdent2 = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "j", 2), "j");
    auto jInit2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "0", 2), 0.0);
    auto initDecl2 = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 2), jIdent2.get(), jInit2.get());
    auto compare2 = std::make_unique<BooleanExpression>(testTokens(TokenType::TRUE, "true", 2), true);
    auto next2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "0", 2), 0.0);
    auto loopBlock2 = std::make_unique<BlockStatement>(testTokens(2), std::vector<Statement*>{});
    auto forStmt2 = std::make_unique<ForStatement>(testTokens(TokenType::FOR, "for", 2),
        initDecl2.get(), compare2.get(), next2.get(), loopBlock2.get());

    auto outerBlock = std::make_unique<BlockStatement>(testTokens(1), std::vector<Statement*>{ forStmt1.get(), forStmt2.get() });

    SyntaxNode* rootRaw = outerBlock.get();

    tree.add(std::move(jIdent1));
    tree.add(std::move(jInit1));
    tree.add(std::move(initDecl1));
    tree.add(std::move(compare1));
    tree.add(std::move(next1));
    tree.add(std::move(loopBlock1));
    tree.add(std::move(forStmt1));
    tree.add(std::move(jIdent2));
    tree.add(std::move(jInit2));
    tree.add(std::move(initDecl2));
    tree.add(std::move(compare2));
    tree.add(std::move(next2));
    tree.add(std::move(loopBlock2));
    tree.add(std::move(forStmt2));
    tree.add(std::move(outerBlock));
    tree.setRoot(rootRaw);

    EXPECT_TRUE(checker.check(tree));
}

// ===========================================================================
// 함수
// ===========================================================================

// return;   (함수 밖)
TEST_F(CheckerTest, ReturnOutsideFunctionReportsError) {
    auto ret = std::make_unique<ReturnStatement>(testTokens(TokenType::RETURN, "return", 1));
    SyntaxNode* root = ret.get();
    tree.add(std::move(ret));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("함수(메서드) 밖에서 return"));
    }
}

// Func f(a, a) { }   (파라미터 이름 중복)
TEST_F(CheckerTest, DuplicateParameterNameReportsError) {
    Token nameToken{ TokenType::IDENTIFIER, "f", 1 };
    std::vector<Token> params{
        Token{ TokenType::IDENTIFIER, "a", 1 },
        Token{ TokenType::IDENTIFIER, "a", 1 }
    };
    auto funcDecl = std::make_unique<FunctionDeclareStatement>(
        testTokens(TokenType::FUNC, "Func", 1), nameToken, params, std::vector<Statement*>{});
    SyntaxNode* root = funcDecl.get();
    tree.add(std::move(funcDecl));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("파라미터 이름 'a'"));
    }
}

// Func fact(n) { return fact(n); }
// 함수 이름을 바디 검사 "전에" 등록해야 재귀 호출이 미선언 변수 오류로 잘못 걸리지 않는다.
TEST_F(CheckerTest, RecursiveFunctionCallPasses) {
    Token factName{ TokenType::IDENTIFIER, "fact", 1 };
    Token nParam{ TokenType::IDENTIFIER, "n", 1 };

    auto calleeId = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "fact", 2), "fact");
    auto argId = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "n", 2), "n");
    std::vector<Expression*> args{ argId.get() };
    auto callExpr = std::make_unique<CallExpression>(testTokens(2), calleeId.get(), args);
    auto retStmt = std::make_unique<ReturnStatement>(testTokens(TokenType::RETURN, "return", 2), callExpr.get());

    std::vector<Statement*> body{ retStmt.get() };
    auto funcDecl = std::make_unique<FunctionDeclareStatement>(
        testTokens(TokenType::FUNC, "Func", 1), factName, std::vector<Token>{ nParam }, body);

    SyntaxNode* root = funcDecl.get();

    tree.add(std::move(calleeId));
    tree.add(std::move(argId));
    tree.add(std::move(callExpr));
    tree.add(std::move(retStmt));
    tree.add(std::move(funcDecl));
    tree.setRoot(root);

    EXPECT_TRUE(checker.check(tree));
}

// ===========================================================================
// 클래스
// ===========================================================================

// print This;   (클래스 밖)
TEST_F(CheckerTest, ThisOutsideClassReportsError) {
    auto thisExpr = std::make_unique<ThisExpression>(testTokens(TokenType::THIS, "This", 1));
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), thisExpr.get());
    SyntaxNode* root = printStmt.get();
    tree.add(std::move(thisExpr));
    tree.add(std::move(printStmt));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("클래스 메서드 밖에서 This"));
    }
}

// Class C { m() { print This; } }
TEST_F(CheckerTest, ThisInsideMethodPasses) {
    Token className{ TokenType::IDENTIFIER, "C", 1 };
    Token methodName{ TokenType::IDENTIFIER, "m", 1 };

    auto thisExpr = std::make_unique<ThisExpression>(testTokens(TokenType::THIS, "This", 2));
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 2), thisExpr.get());
    std::vector<Statement*> body{ printStmt.get() };
    auto method = std::make_unique<MethodDeclareStatement>(testTokens(2), methodName, std::vector<Token>{}, body);
    std::vector<MethodDeclareStatement*> methods{ method.get() };
    auto classDecl = std::make_unique<ClassDeclareStatement>(
        testTokens(TokenType::CLASS, "Class", 1), className, methods);

    SyntaxNode* root = classDecl.get();
    tree.add(std::move(thisExpr));
    tree.add(std::move(printStmt));
    tree.add(std::move(method));
    tree.add(std::move(classDecl));
    tree.setRoot(root);

    EXPECT_TRUE(checker.check(tree));
}

// Class C { init() { return 1; } }   (init은 값 있는 return 금지)
TEST_F(CheckerTest, InitMethodWithReturnValueReportsError) {
    Token className{ TokenType::IDENTIFIER, "C", 1 };
    Token initName{ TokenType::IDENTIFIER, "init", 1 };

    auto retValue = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 2), 1.0);
    auto retStmt = std::make_unique<ReturnStatement>(testTokens(TokenType::RETURN, "return", 2), retValue.get());
    std::vector<Statement*> body{ retStmt.get() };
    auto method = std::make_unique<MethodDeclareStatement>(testTokens(2), initName, std::vector<Token>{}, body);
    std::vector<MethodDeclareStatement*> methods{ method.get() };
    auto classDecl = std::make_unique<ClassDeclareStatement>(
        testTokens(TokenType::CLASS, "Class", 1), className, methods);

    SyntaxNode* root = classDecl.get();
    tree.add(std::move(retValue));
    tree.add(std::move(retStmt));
    tree.add(std::move(method));
    tree.add(std::move(classDecl));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("init 메서드는 값을 반환할 수 없습니다"));
    }
}

// Class C { init() { return; } }   (값 없는 return은 init에서도 허용)
TEST_F(CheckerTest, InitMethodWithBareReturnPasses) {
    Token className{ TokenType::IDENTIFIER, "C", 1 };
    Token initName{ TokenType::IDENTIFIER, "init", 1 };

    auto retStmt = std::make_unique<ReturnStatement>(testTokens(TokenType::RETURN, "return", 2));
    std::vector<Statement*> body{ retStmt.get() };
    auto method = std::make_unique<MethodDeclareStatement>(testTokens(2), initName, std::vector<Token>{}, body);
    std::vector<MethodDeclareStatement*> methods{ method.get() };
    auto classDecl = std::make_unique<ClassDeclareStatement>(
        testTokens(TokenType::CLASS, "Class", 1), className, methods);

    SyntaxNode* root = classDecl.get();
    tree.add(std::move(retStmt));
    tree.add(std::move(method));
    tree.add(std::move(classDecl));
    tree.setRoot(root);

    EXPECT_TRUE(checker.check(tree));
}

// ===========================================================================
// import
// ===========================================================================

// for (0; true; 0) { import "m" alias m; }   (for 안 import 금지)
TEST_F(CheckerTest, ImportInsideForBlockReportsError) {
    auto initExpr = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "0", 1), 0.0);
    auto init = std::make_unique<ExpressionStatement>(testTokens(1), initExpr.get());
    auto compare = std::make_unique<BooleanExpression>(testTokens(TokenType::TRUE, "true", 1), true);
    auto next = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "0", 1), 0.0);

    Token alias{ TokenType::IDENTIFIER, "m", 2 };
    auto importStmt = std::make_unique<ImportStatement>(
        testTokens(TokenType::IMPORT, "import", 2), alias, std::vector<Statement*>{});
    std::vector<Statement*> loopStmts{ importStmt.get() };
    auto loopBlock = std::make_unique<BlockStatement>(testTokens(1), loopStmts);

    auto forStmt = std::make_unique<ForStatement>(testTokens(TokenType::FOR, "for", 1),
        init.get(), compare.get(), next.get(), loopBlock.get());

    SyntaxNode* root = forStmt.get();
    tree.add(std::move(initExpr));
    tree.add(std::move(init));
    tree.add(std::move(compare));
    tree.add(std::move(next));
    tree.add(std::move(importStmt));
    tree.add(std::move(loopBlock));
    tree.add(std::move(forStmt));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("반복문(for) 안에서는 import를 사용할 수 없습니다"));
    }
}

// { import "a" alias m; import "b" alias m; }   (같은 스코프 alias 중복)
TEST_F(CheckerTest, DuplicateImportAliasInSameScopeReportsError) {
    Token alias1{ TokenType::IDENTIFIER, "m", 1 };
    auto import1 = std::make_unique<ImportStatement>(
        testTokens(TokenType::IMPORT, "import", 1), alias1, std::vector<Statement*>{});

    Token alias2{ TokenType::IDENTIFIER, "m", 2 };
    auto import2 = std::make_unique<ImportStatement>(
        testTokens(TokenType::IMPORT, "import", 2), alias2, std::vector<Statement*>{});

    std::vector<Statement*> stmts{ import1.get(), import2.get() };
    auto block = std::make_unique<BlockStatement>(testTokens(1), stmts);

    SyntaxNode* root = block.get();
    tree.add(std::move(import1));
    tree.add(std::move(import2));
    tree.add(std::move(block));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("'m'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다"));
    }
}

// import "sum.txt" alias sum; { import "sum.txt" alias sum; }
// 상위 스코프에서 이미 import된 이름을 하위 스코프에서 다시 import하면 에러 (PDF p.27)
TEST_F(CheckerTest, ImportInNestedScopeWhenAlreadyImportedInOuterScopeReportsError) {
    Token alias1{ TokenType::IDENTIFIER, "sum", 1 };
    auto import1 = std::make_unique<ImportStatement>(
        testTokens(TokenType::IMPORT, "import", 1), alias1, std::vector<Statement*>{});

    Token alias2{ TokenType::IDENTIFIER, "sum", 2 };
    auto import2 = std::make_unique<ImportStatement>(
        testTokens(TokenType::IMPORT, "import", 2), alias2, std::vector<Statement*>{});
    std::vector<Statement*> innerStmts{ import2.get() };
    auto innerBlock = std::make_unique<BlockStatement>(testTokens(2), innerStmts);

    std::vector<Statement*> outerStmts{ import1.get(), innerBlock.get() };
    auto outerBlock = std::make_unique<BlockStatement>(testTokens(1), outerStmts);

    SyntaxNode* root = outerBlock.get();
    tree.add(std::move(import1));
    tree.add(std::move(import2));
    tree.add(std::move(innerBlock));
    tree.add(std::move(outerBlock));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("'sum'에러: 상위 스코프에서 이미 사용중인 이름입니다"));
    }
}

// { import "sum.txt" alias sum; } { import "sum.txt" alias sum; }
// 서로 다른 형제 스코프에서는 같은 이름을 다시 import해도 된다 (PDF p.27)
TEST_F(CheckerTest, ImportSameAliasInSiblingScopesPasses) {
    Token alias1{ TokenType::IDENTIFIER, "sum", 1 };
    auto import1 = std::make_unique<ImportStatement>(
        testTokens(TokenType::IMPORT, "import", 1), alias1, std::vector<Statement*>{});
    std::vector<Statement*> block1Stmts{ import1.get() };
    auto block1 = std::make_unique<BlockStatement>(testTokens(1), block1Stmts);

    Token alias2{ TokenType::IDENTIFIER, "sum", 2 };
    auto import2 = std::make_unique<ImportStatement>(
        testTokens(TokenType::IMPORT, "import", 2), alias2, std::vector<Statement*>{});
    std::vector<Statement*> block2Stmts{ import2.get() };
    auto block2 = std::make_unique<BlockStatement>(testTokens(2), block2Stmts);

    std::vector<Statement*> rootStmts{ block1.get(), block2.get() };
    auto rootBlock = std::make_unique<BlockStatement>(testTokens(1), rootStmts);

    SyntaxNode* root = rootBlock.get();
    tree.add(std::move(import1));
    tree.add(std::move(block1));
    tree.add(std::move(import2));
    tree.add(std::move(block2));
    tree.add(std::move(rootBlock));
    tree.setRoot(root);

    EXPECT_TRUE(checker.check(tree));
}

// { import "sum.txt" alias sum; } import "sum.txt" alias sum;
// 블록이 끝난 뒤 같은 레벨에서 같은 이름을 다시 import해도 된다 (PDF p.27)
TEST_F(CheckerTest, ImportSameAliasAfterBlockEndsPasses) {
    Token alias1{ TokenType::IDENTIFIER, "sum", 1 };
    auto import1 = std::make_unique<ImportStatement>(
        testTokens(TokenType::IMPORT, "import", 1), alias1, std::vector<Statement*>{});
    std::vector<Statement*> block1Stmts{ import1.get() };
    auto block1 = std::make_unique<BlockStatement>(testTokens(1), block1Stmts);

    Token alias2{ TokenType::IDENTIFIER, "sum", 2 };
    auto import2 = std::make_unique<ImportStatement>(
        testTokens(TokenType::IMPORT, "import", 2), alias2, std::vector<Statement*>{});

    std::vector<Statement*> rootStmts{ block1.get(), import2.get() };
    auto rootBlock = std::make_unique<BlockStatement>(testTokens(1), rootStmts);

    SyntaxNode* root = rootBlock.get();
    tree.add(std::move(import1));
    tree.add(std::move(block1));
    tree.add(std::move(import2));
    tree.add(std::move(rootBlock));
    tree.setRoot(root);

    EXPECT_TRUE(checker.check(tree));
}

// { var sum = 1; import "sum.txt" alias sum; }   (alias가 기존 변수 이름과 충돌, PDF p.28)
TEST_F(CheckerTest, ImportAliasConflictsWithExistingVariableReportsError) {
    auto declIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "sum", 1), "sum");
    auto lit = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto declSum = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 1), declIdent.get(), lit.get());

    Token alias{ TokenType::IDENTIFIER, "sum", 2 };
    auto importStmt = std::make_unique<ImportStatement>(
        testTokens(TokenType::IMPORT, "import", 2), alias, std::vector<Statement*>{});

    std::vector<Statement*> stmts{ declSum.get(), importStmt.get() };
    auto block = std::make_unique<BlockStatement>(testTokens(1), stmts);

    SyntaxNode* root = block.get();
    tree.add(std::move(declIdent));
    tree.add(std::move(lit));
    tree.add(std::move(declSum));
    tree.add(std::move(importStmt));
    tree.add(std::move(block));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("'sum'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다"));
    }
}

// ===========================================================================
// Resolver (정적 바인딩)
// ===========================================================================

// { var a = 1; print a; }   -> a는 현재(같은) 스코프에서 바로 찾으므로 depth == 0
TEST_F(CheckerTest, ResolverAssignsDepthZeroForSameScopeVariable) {
    auto declIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 1), "a");
    auto lit = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto declA = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 1), declIdent.get(), lit.get());

    auto usageId = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 2), "a");
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 2), usageId.get());

    std::vector<Statement*> stmts{ declA.get(), printStmt.get() };
    auto block = std::make_unique<BlockStatement>(testTokens(1), stmts);

    SyntaxNode* root = block.get();
    IdentifierExpression* usageRaw = usageId.get();

    tree.add(std::move(declIdent));
    tree.add(std::move(lit));
    tree.add(std::move(declA));
    tree.add(std::move(usageId));
    tree.add(std::move(printStmt));
    tree.add(std::move(block));
    tree.setRoot(root);

    ASSERT_TRUE(checker.check(tree));

    ASSERT_TRUE(usageRaw->depth.has_value());
    EXPECT_EQ(0, usageRaw->depth.value());
}

// { var a = 1; { print a; } }   -> a는 한 단계 바깥 스코프에 있으므로 depth == 1
TEST_F(CheckerTest, ResolverAssignsDepthOneForOuterScopeVariable) {
    auto declIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 1), "a");
    auto lit = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto declA = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 1), declIdent.get(), lit.get());

    auto usageId = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 2), "a");
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 2), usageId.get());
    std::vector<Statement*> innerStmts{ printStmt.get() };
    auto innerBlock = std::make_unique<BlockStatement>(testTokens(2), innerStmts);

    std::vector<Statement*> outerStmts{ declA.get(), innerBlock.get() };
    auto outerBlock = std::make_unique<BlockStatement>(testTokens(1), outerStmts);

    SyntaxNode* root = outerBlock.get();
    IdentifierExpression* usageRaw = usageId.get();

    tree.add(std::move(declIdent));
    tree.add(std::move(lit));
    tree.add(std::move(declA));
    tree.add(std::move(usageId));
    tree.add(std::move(printStmt));
    tree.add(std::move(innerBlock));
    tree.add(std::move(outerBlock));
    tree.setRoot(root);

    ASSERT_TRUE(checker.check(tree));

    ASSERT_TRUE(usageRaw->depth.has_value());
    EXPECT_EQ(1, usageRaw->depth.value());
}

// ===========================================================================
// import 스코프 누락 수정 (checkImport, PR #36 지적사항)
// ===========================================================================

// { import "sum.txt" alias sum; print inner; }   -> import 내부 선언(inner)이 바깥
// 스코프로 새어나가면 안 된다. sum은 보이지만 inner는 선언되지 않은 변수여야 한다.
TEST_F(CheckerTest, ImportInternalDeclarationDoesNotLeakIntoEnclosingScope) {
    auto innerIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "inner", 1), "inner");
    auto innerLit = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto innerDecl = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 1), innerIdent.get(), innerLit.get());

    Token alias{ TokenType::IDENTIFIER, "sum", 1 };
    auto importStmt = std::make_unique<ImportStatement>(
        testTokens(TokenType::IMPORT, "import", 1), alias, std::vector<Statement*>{ innerDecl.get() });

    auto usageInner = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "inner", 2), "inner");
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 2), usageInner.get());

    std::vector<Statement*> stmts{ importStmt.get(), printStmt.get() };
    auto block = std::make_unique<BlockStatement>(testTokens(1), stmts);

    SyntaxNode* root = block.get();
    tree.add(std::move(innerIdent));
    tree.add(std::move(innerLit));
    tree.add(std::move(innerDecl));
    tree.add(std::move(importStmt));
    tree.add(std::move(usageInner));
    tree.add(std::move(printStmt));
    tree.add(std::move(block));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("'inner'에러: 선언되지 않은 변수입니다"));
    }
}

// ===========================================================================
// 상속 의미 검사 (결정 2: 클래스가 아닌 대상 상속 금지 / 자기 자신 상속 금지)
// ===========================================================================

// Class A : A { }   -> 자기 자신을 상속할 수 없다.
TEST_F(CheckerTest, ClassInheritingFromItselfReportsError) {
    Token className{ TokenType::IDENTIFIER, "A", 1 };
    auto superIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "A", 1), "A");
    auto classDecl = std::make_unique<ClassDeclareStatement>(
        testTokens(TokenType::CLASS, "Class", 1), className, std::vector<MethodDeclareStatement*>{}, superIdent.get());

    SyntaxNode* root = classDecl.get();
    tree.add(std::move(superIdent));
    tree.add(std::move(classDecl));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("자기 자신을 상속할 수 없습니다"));
    }
}

// var x = 10; Class B : x { }   -> 클래스가 아닌 대상은 상속할 수 없다.
TEST_F(CheckerTest, ClassInheritingFromNonClassReportsError) {
    auto declIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "x", 1), "x");
    auto lit = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "10", 1), 10.0);
    auto declX = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 1), declIdent.get(), lit.get());

    Token className{ TokenType::IDENTIFIER, "B", 2 };
    auto superIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "x", 2), "x");
    auto classDecl = std::make_unique<ClassDeclareStatement>(
        testTokens(TokenType::CLASS, "Class", 2), className, std::vector<MethodDeclareStatement*>{}, superIdent.get());

    std::vector<Statement*> stmts{ declX.get(), classDecl.get() };
    auto block = std::make_unique<BlockStatement>(testTokens(1), stmts);

    SyntaxNode* root = block.get();
    tree.add(std::move(declIdent));
    tree.add(std::move(lit));
    tree.add(std::move(declX));
    tree.add(std::move(superIdent));
    tree.add(std::move(classDecl));
    tree.add(std::move(block));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("'x'은(는) 클래스가 아니므로 상속할 수 없습니다"));
    }
}

// Class A { } Class B : A { }   -> 정상적으로 선언된 클래스 상속은 통과한다.
TEST_F(CheckerTest, ClassInheritingFromDeclaredClassPasses) {
    Token classAName{ TokenType::IDENTIFIER, "A", 1 };
    auto classA = std::make_unique<ClassDeclareStatement>(
        testTokens(TokenType::CLASS, "Class", 1), classAName, std::vector<MethodDeclareStatement*>{});

    Token classBName{ TokenType::IDENTIFIER, "B", 2 };
    auto superIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "A", 2), "A");
    auto classB = std::make_unique<ClassDeclareStatement>(
        testTokens(TokenType::CLASS, "Class", 2), classBName, std::vector<MethodDeclareStatement*>{}, superIdent.get());

    std::vector<Statement*> stmts{ classA.get(), classB.get() };
    auto block = std::make_unique<BlockStatement>(testTokens(1), stmts);

    SyntaxNode* root = block.get();
    tree.add(std::move(classA));
    tree.add(std::move(superIdent));
    tree.add(std::move(classB));
    tree.add(std::move(block));
    tree.setRoot(root);

    EXPECT_TRUE(checker.check(tree));
}

// print Super;   (클래스 밖) -> Super는 클래스 메서드 밖에서 쓸 수 없다.
TEST_F(CheckerTest, SuperOutsideClassReportsError) {
    auto superExpr = std::make_unique<SuperExpression>(testTokens(TokenType::SUPER, "Super", 1));
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), superExpr.get());
    SyntaxNode* root = printStmt.get();
    tree.add(std::move(superExpr));
    tree.add(std::move(printStmt));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("클래스 메서드 밖에서 Super"));
    }
}

// Class C { m() { print Super; } }   (부모 없는 클래스) -> Super를 쓸 수 없다.
TEST_F(CheckerTest, SuperInsideMethodWithoutSuperclassReportsError) {
    Token className{ TokenType::IDENTIFIER, "C", 1 };
    Token methodName{ TokenType::IDENTIFIER, "m", 1 };

    auto superExpr = std::make_unique<SuperExpression>(testTokens(TokenType::SUPER, "Super", 2));
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 2), superExpr.get());
    std::vector<Statement*> body{ printStmt.get() };
    auto method = std::make_unique<MethodDeclareStatement>(testTokens(2), methodName, std::vector<Token>{}, body);
    std::vector<MethodDeclareStatement*> methods{ method.get() };
    auto classDecl = std::make_unique<ClassDeclareStatement>(
        testTokens(TokenType::CLASS, "Class", 1), className, methods);

    SyntaxNode* root = classDecl.get();
    tree.add(std::move(superExpr));
    tree.add(std::move(printStmt));
    tree.add(std::move(method));
    tree.add(std::move(classDecl));
    tree.setRoot(root);

    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("부모 클래스가 없는 클래스에서 Super"));
    }
}

// Class A { } Class B : A { m() { print Super; } }   (부모 있는 클래스) -> 통과한다.
TEST_F(CheckerTest, SuperInsideMethodWithSuperclassPasses) {
    Token classAName{ TokenType::IDENTIFIER, "A", 1 };
    auto classA = std::make_unique<ClassDeclareStatement>(
        testTokens(TokenType::CLASS, "Class", 1), classAName, std::vector<MethodDeclareStatement*>{});

    Token classBName{ TokenType::IDENTIFIER, "B", 2 };
    Token methodName{ TokenType::IDENTIFIER, "m", 2 };
    auto superIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "A", 2), "A");

    auto superExpr = std::make_unique<SuperExpression>(testTokens(TokenType::SUPER, "Super", 3));
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 3), superExpr.get());
    std::vector<Statement*> body{ printStmt.get() };
    auto method = std::make_unique<MethodDeclareStatement>(testTokens(2), methodName, std::vector<Token>{}, body);
    std::vector<MethodDeclareStatement*> methods{ method.get() };
    auto classB = std::make_unique<ClassDeclareStatement>(
        testTokens(TokenType::CLASS, "Class", 2), classBName, methods, superIdent.get());

    std::vector<Statement*> stmts{ classA.get(), classB.get() };
    auto block = std::make_unique<BlockStatement>(testTokens(1), stmts);

    SyntaxNode* root = block.get();
    tree.add(std::move(classA));
    tree.add(std::move(superIdent));
    tree.add(std::move(superExpr));
    tree.add(std::move(printStmt));
    tree.add(std::move(method));
    tree.add(std::move(classB));
    tree.add(std::move(block));
    tree.setRoot(root);

    EXPECT_TRUE(checker.check(tree));
}
