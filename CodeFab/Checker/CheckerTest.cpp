#include <sstream>

#include "gmock/gmock.h"
#include "Checker.h"
#include "../Executor/Executor.h"

// ---------------------------------------------------------------------------
// 테스트용 mock 트리 생성 헬퍼
// ---------------------------------------------------------------------------
// tokenizer/assembler가 만든 진짜 트리 대신, checker를 독립적으로 검증하기 위해
// SyntaxNode 트리를 직접 손으로 구성한다.

// 대표 토큰의 type/origin이 검사에 의미가 없는 구조적 노드(BlockStatement 등)용 더미.
static std::vector<Token> testTokens(int line) {
    return { Token{ TokenType::NUMBER, "", line } };
}

// 실제 토큰(리터럴/연산자/키워드/식별자)에 정확히 대응시키고 싶을 때 사용.
static std::vector<Token> testTokens(TokenType type, const std::string& origin, int line) {
    return { Token{ type, origin, line } };
}

// ---------------------------------------------------------------------------
// print 1 + 2 * 3;   // expect: 7
// ---------------------------------------------------------------------------
TEST(CheckerTest, PrintExpressionWithNoErrorsPasses) {
    SyntaxTree tree;

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

    std::ostringstream executorOutput;
    Executor executor(executorOutput);
    Checker checker(executor);
    EXPECT_TRUE(checker.check(tree));
}

// ---------------------------------------------------------------------------
// { var a = 10; var a = 12; }   // 2번째 var a 에서 중복 선언 에러 (p.72)
// ---------------------------------------------------------------------------
TEST(CheckerTest, DuplicateDeclarationInSameScopeReportsError) {
    SyntaxTree tree;

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

    std::ostringstream executorOutput;
    Executor executor(executorOutput);
    Checker checker(executor);
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

// ---------------------------------------------------------------------------
// { var a = a + 1; }   // 자신의 초기화식에서 자기 자신을 참조 -> 에러 (p.73)
// ---------------------------------------------------------------------------
TEST(CheckerTest, SelfReferenceInInitializerReportsError) {
    SyntaxTree tree;

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

    std::ostringstream executorOutput;
    Executor executor(executorOutput);
    Checker checker(executor);
    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("2번째 줄"));
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("자신의 초기화식에서 지역변수를 읽을 수 없습니다"));
    }
}

// ---------------------------------------------------------------------------
// var a = 10;   (1번째 check() 호출)
// print a;      (2번째 check() 호출, 같은 Checker 인스턴스)
//
// RunPromptShell은 REPL 한 줄마다 check()를 새로 호출하지만 Checker는 Executor의
// Environment처럼 세션 전체에서 재사용되는 하나의 인스턴스다. 그래서 첫 호출에서
// 선언한 전역 변수는 다음 호출에서도 "선언된 변수"로 남아있어야 한다.
// ---------------------------------------------------------------------------
TEST(CheckerTest, VariableDeclaredInEarlierCallIsVisibleToLaterCall) {
    std::ostringstream executorOutput;
    Executor executor(executorOutput);
    Checker checker(executor);

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

// ---------------------------------------------------------------------------
// print notDefined;   (선언된 적 없는 변수 - 세션 전체에 걸쳐도 여전히 에러여야 한다)
// ---------------------------------------------------------------------------
TEST(CheckerTest, UndefinedVariableAcrossCallsStillReportsError) {
    std::ostringstream executorOutput;
    Executor executor(executorOutput);
    Checker checker(executor);

    SyntaxTree tree;
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

// ---------------------------------------------------------------------------
// if (true) { var a = 1; var a = 2; }
// checkStatement가 IfStatement의 thenBranch까지 재귀해서 내부 블록도 검사하는지 확인한다.
// ---------------------------------------------------------------------------
TEST(CheckerTest, DuplicateDeclarationInsideIfBlockReportsError) {
    SyntaxTree tree;

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

    std::ostringstream executorOutput;
    Executor executor(executorOutput);
    Checker checker(executor);
    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("3번째 줄"));
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("이미 해당 변수는 현재 스코프에서 사용중입니다"));
    }
}

// ---------------------------------------------------------------------------
// if (true) { var a = a + 1; }
// checkStatement가 IfStatement의 thenBranch 안 자기 참조도 잡아내는지 확인한다.
// ---------------------------------------------------------------------------
TEST(CheckerTest, SelfReferenceInsideIfBlockReportsError) {
    SyntaxTree tree;

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

    std::ostringstream executorOutput;
    Executor executor(executorOutput);
    Checker checker(executor);
    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("2번째 줄"));
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("자신의 초기화식에서 지역변수를 읽을 수 없습니다"));
    }
}

// ---------------------------------------------------------------------------
// for (var j = 0; true; 0) { var a = 1; var a = 2; }
// checkStatement가 ForStatement의 loop 본문까지 재귀해서 내부 블록도 검사하는지,
// 그리고 init에서 선언한 j가 for 전용 스코프에 들어가는지 함께 확인한다.
// ---------------------------------------------------------------------------
TEST(CheckerTest, DuplicateDeclarationInsideForBlockReportsError) {
    SyntaxTree tree;

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

    std::ostringstream executorOutput;
    Executor executor(executorOutput);
    Checker checker(executor);
    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("3번째 줄"));
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("이미 해당 변수는 현재 스코프에서 사용중입니다"));
    }
}

// ---------------------------------------------------------------------------
// for (var j = 0; true; 0) { var a = a + 1; }
// checkStatement가 ForStatement의 loop 본문 안 자기 참조도 잡아내는지 확인한다.
// ---------------------------------------------------------------------------
TEST(CheckerTest, SelfReferenceInsideForBlockReportsError) {
    SyntaxTree tree;

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

    std::ostringstream executorOutput;
    Executor executor(executorOutput);
    Checker checker(executor);
    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("2번째 줄"));
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("자신의 초기화식에서 지역변수를 읽을 수 없습니다"));
    }
}

// ---------------------------------------------------------------------------
// for (var j = 0; ...) { ... }  다음 for (var j = 0; ...) { ... }
// for의 init에서 선언한 루프 변수는 전용 스코프에 갇혀야 하므로, 같은 이름의 루프
// 변수를 쓰는 두 개의 for문이 연달아 있어도 "중복 선언" 오류가 나면 안 된다.
// ---------------------------------------------------------------------------
TEST(CheckerTest, ForLoopVariableDoesNotLeakIntoEnclosingScope) {
    SyntaxTree tree;

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

    std::ostringstream executorOutput;
    Executor executor(executorOutput);
    Checker checker(executor);
    EXPECT_TRUE(checker.check(tree));
}
