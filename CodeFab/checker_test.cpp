#include "gmock/gmock.h"
#include "checker.h"

// ---------------------------------------------------------------------------
// 테스트용 mock 트리 생성 헬퍼
// ---------------------------------------------------------------------------
// tokenizer/assembler가 만든 진짜 트리 대신, checker를 독립적으로 검증하기 위해
// SyntaxNode 트리를 직접 손으로 구성한다.
// 노드 생성자는 vector<Token>을 요구하지만 checker는 tokens의 "줄 번호"만 사용하므로,
// 지정한 line 하나만 담긴 더미 토큰을 넣어준다.

static std::vector<Token> testTokens(int line) {
    return { Token{ NUMBER, "", line } };
}

// ---------------------------------------------------------------------------
// print 1 + 2 * 3;   // expect: 7
// ---------------------------------------------------------------------------
TEST(CheckerTest, PrintExpressionWithNoErrors_Passes) {
    SyntaxTree tree;

    auto lit1 = std::make_unique<LiteralExpression>(testTokens(1));
    lit1->value = 1;
    auto lit2 = std::make_unique<LiteralExpression>(testTokens(1));
    lit2->value = 2;
    auto lit3 = std::make_unique<LiteralExpression>(testTokens(1));
    lit3->value = 3;

    auto mult = std::make_unique<MultExpression>(testTokens(1));
    mult->left = lit2.get();
    mult->right = lit3.get();

    auto add = std::make_unique<AddExpression>(testTokens(1));
    add->left = lit1.get();
    add->right = mult.get();

    auto printStmt = std::make_unique<PrintStatement>(testTokens(1));
    printStmt->expr = add.get();

    auto block = std::make_unique<BlockStatement>(testTokens(1));
    block->statements.push_back(printStmt.get());

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

    Checker checker;
    CheckResult result = checker.check(tree);

    EXPECT_TRUE(result.passed);
    EXPECT_EQ(0u, result.errors.size());
}

// ---------------------------------------------------------------------------
// { var a = 10; var a = 12; }
// ---------------------------------------------------------------------------
TEST(CheckerTest, DuplicateDeclarationInSameScope_ReportsError) {

    SyntaxTree tree;

    auto lit10 = std::make_unique<LiteralExpression>(testTokens(2));
    lit10->value = 10;
    auto declA1 = std::make_unique<VarDeclStatement>(testTokens(2));
    declA1->name = "a";
    declA1->initExpr = lit10.get();

    auto lit12 = std::make_unique<LiteralExpression>(testTokens(3));
    lit12->value = 12;
    auto declA2 = std::make_unique<VarDeclStatement>(testTokens(3));
    declA2->name = "a";
    declA2->initExpr = lit12.get();

    auto block = std::make_unique<BlockStatement>(testTokens(1));
    block->statements.push_back(declA1.get());
    block->statements.push_back(declA2.get());

    SyntaxNode* rootRaw = block.get();

    tree.add(std::move(lit10));
    tree.add(std::move(declA1));
    tree.add(std::move(lit12));
    tree.add(std::move(declA2));
    tree.add(std::move(block));
    tree.setRoot(rootRaw);

    Checker checker;
    CheckResult result = checker.check(tree);

    EXPECT_FALSE(result.passed);
    ASSERT_EQ(1u, result.errors.size());
    EXPECT_THAT(result.errors[0].message, testing::HasSubstr("이미 해당 변수는 현재 스코프에서 사용중입니다"));
    EXPECT_THAT(result.errors[0].message, testing::HasSubstr("3번째 줄"));
}

// ---------------------------------------------------------------------------
// { var a = a + 1; }   // 자신의 초기화식에서 자기 자신을 참조 -> 에러
// ---------------------------------------------------------------------------
TEST(CheckerTest, SelfReferenceInInitializer_ReportsError) {
    SyntaxTree tree;

    auto idA = std::make_unique<IdentifierExpression>(testTokens(2));
    idA->name = "a";
    auto lit1 = std::make_unique<LiteralExpression>(testTokens(2));
    lit1->value = 1;

    auto add = std::make_unique<AddExpression>(testTokens(2));
    add->left = idA.get();
    add->right = lit1.get();

    auto declA = std::make_unique<VarDeclStatement>(testTokens(2));
    declA->name = "a";
    declA->initExpr = add.get();

    auto block = std::make_unique<BlockStatement>(testTokens(1));
    block->statements.push_back(declA.get());

    SyntaxNode* rootRaw = block.get();

    tree.add(std::move(idA));
    tree.add(std::move(lit1));
    tree.add(std::move(add));
    tree.add(std::move(declA));
    tree.add(std::move(block));
    tree.setRoot(rootRaw);

    Checker checker;
    CheckResult result = checker.check(tree);

    EXPECT_FALSE(result.passed);
    ASSERT_EQ(1u, result.errors.size());
    EXPECT_THAT(result.errors[0].message, testing::HasSubstr("자신의 초기화식에서 지역변수를 읽을 수 없습니다"));
    EXPECT_THAT(result.errors[0].message, testing::HasSubstr("2번째 줄"));
}
