#include "gmock/gmock.h"
#include "Checker.h"

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

    Checker checker;
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

    Checker checker;
    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_EQ(3, e.line());
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

    Checker checker;
    try {
        checker.check(tree);
        FAIL() << "CheckerError가 발생해야 합니다.";
    } catch (const CheckerError& e) {
        EXPECT_EQ(2, e.line());
        EXPECT_THAT(std::string(e.what()), testing::HasSubstr("자신의 초기화식에서 지역변수를 읽을 수 없습니다"));
    }
}
