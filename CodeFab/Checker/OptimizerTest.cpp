#include <sstream>

#include "gmock/gmock.h"
#include "Optimizer.h"
#include "../Executor/Executor.h"

// Checker와 같은 헬퍼: 검사 대상 노드 생성용 더미 토큰.
static std::vector<Token> testTokens(int line) {
    return { Token{ TokenType::NUMBER, "", line } };
}

static std::vector<Token> testTokens(TokenType type, const std::string& origin, int line) {
    return { Token{ type, origin, line } };
}

// evaluate() 호출 대상/횟수만 검증하고 싶을 때 쓰는 mock.
class MockExecuteInterface : public ExecuteInterface {
public:
    MOCK_METHOD(void, execute, (SyntaxTree& tree), (override));
    MOCK_METHOD(Value, evaluate, (Expression* expr), (override));
    MOCK_METHOD(const Environment&, environment, (), (const, override));
};

// 실제 계산값을 확인하고 싶은 테스트(트리가 진짜로 리터럴로 치환됐는지)는 실제 Executor를 쓴다.
class OptimizerTest : public ::testing::Test {
protected:
    SyntaxTree tree;
    std::ostringstream executorOutput;
    Executor executor{ executorOutput };
    Optimizer optimizer{ executor };
};

// evaluate() 호출 여부/횟수만 확인하고 싶은 테스트는 mock을 주입한다.
class OptimizerMockTest : public ::testing::Test {
protected:
    SyntaxTree tree;
    MockExecuteInterface executor;
    Optimizer optimizer{ executor };
};

// print 1 + 2;   -> 두 자식이 모두 리터럴이므로 evaluate()가 정확히 한 번 호출된다.
// AddExpression 자신은 PrintStatement::expr(const)에 물려 있어 트리 치환은 되지 않는다.
TEST_F(OptimizerMockTest, FoldsTopLevelLiteralBinaryExpressionByCallingEvaluate) {
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto lit2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "2", 1), 2.0);
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 1), lit1.get(), lit2.get());
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), add.get());

    SyntaxNode* root = printStmt.get();
    AddExpression* addRaw = add.get();
    tree.add(std::move(lit1));
    tree.add(std::move(lit2));
    tree.add(std::move(add));
    tree.add(std::move(printStmt));
    tree.setRoot(root);

    EXPECT_CALL(executor, evaluate(static_cast<Expression*>(addRaw)))
        .Times(1)
        .WillOnce(testing::Return(Value(3.0)));

    optimizer.optimize(tree);
}

// print 1 + 2 * 3;   -> mult(2, 3)는 BinaryExpression::right(비-const) 자리에 있으므로
// 폴딩되면 실제로 add->right가 리터럴 6으로 치환된다. 치환된 뒤에는 add 자신도
// (리터럴, 리터럴)이 되어 evaluate()가 다시 호출된다 - 총 2회.
TEST_F(OptimizerTest, ReplacesNestedBinaryExpressionWithFoldedLiteral) {
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto lit2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "2", 1), 2.0);
    auto lit3 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "3", 1), 3.0);
    auto mult = std::make_unique<MultExpression>(testTokens(TokenType::STAR, "*", 1), lit2.get(), lit3.get());
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 1), lit1.get(), mult.get());
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), add.get());

    SyntaxNode* root = printStmt.get();
    AddExpression* addRaw = add.get();
    tree.add(std::move(lit1));
    tree.add(std::move(lit2));
    tree.add(std::move(lit3));
    tree.add(std::move(mult));
    tree.add(std::move(add));
    tree.add(std::move(printStmt));
    tree.setRoot(root);

    optimizer.optimize(tree);

    auto* foldedRight = dynamic_cast<NumberExpression*>(addRaw->right);
    ASSERT_NE(nullptr, foldedRight) << "mult(2, 3)가 리터럴로 치환돼야 합니다.";
    EXPECT_EQ(6.0, foldedRight->value);
}

// print a + 2;   -> a가 변수(리터럴이 아님)라 폴딩 대상이 아니다. evaluate()가 호출되면 안 된다.
TEST_F(OptimizerMockTest, DoesNotCallEvaluateWhenOperandIsVariable) {
    auto declIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 1), "a");
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto declA = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 1), declIdent.get(), lit1.get());

    auto usageA = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 2), "a");
    auto lit2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "2", 2), 2.0);
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 2), usageA.get(), lit2.get());
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 2), add.get());

    std::vector<Statement*> stmts{ declA.get(), printStmt.get() };
    auto block = std::make_unique<BlockStatement>(testTokens(1), stmts);

    SyntaxNode* root = block.get();
    tree.add(std::move(declIdent));
    tree.add(std::move(lit1));
    tree.add(std::move(declA));
    tree.add(std::move(usageA));
    tree.add(std::move(lit2));
    tree.add(std::move(add));
    tree.add(std::move(printStmt));
    tree.add(std::move(block));
    tree.setRoot(root);

    EXPECT_CALL(executor, evaluate(testing::_)).Times(0);

    optimizer.optimize(tree);
}

// print 1 + (-3);   -> NegativeExpression(3)이 AddExpression::right(비-const) 자리에
// 있으므로 단항 폴딩(foldUnary)으로 -3 리터럴로 치환되고, 그 결과 add 자신도
// (리터럴, 리터럴)이 되어 다시 폴딩된다 - Visitor로 옮긴 뒤에도 NegativeExpression/
// AddExpression 각각의 visit()이 정상적으로 디스패치되는지 확인한다.
TEST_F(OptimizerTest, FoldsUnaryNegationNestedInsideBinaryExpression) {
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto lit3 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "3", 1), 3.0);
    auto neg = std::make_unique<NegativeExpression>(testTokens(TokenType::MINUS, "-", 1), lit3.get());
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 1), lit1.get(), neg.get());
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), add.get());

    SyntaxNode* root = printStmt.get();
    AddExpression* addRaw = add.get();
    tree.add(std::move(lit1));
    tree.add(std::move(lit3));
    tree.add(std::move(neg));
    tree.add(std::move(add));
    tree.add(std::move(printStmt));
    tree.setRoot(root);

    optimizer.optimize(tree);

    auto* foldedRight = dynamic_cast<NumberExpression*>(addRaw->right);
    ASSERT_NE(nullptr, foldedRight) << "-3이 리터럴로 치환돼야 합니다.";
    EXPECT_EQ(-3.0, foldedRight->value);
}

// if (true) { print 1 + 2; }   -> IfStatement/BlockStatement를 거쳐 재귀적으로
// 방문해도 그 안의 AddExpression이 여전히 폴딩 대상으로 잡히는지 확인한다(Visitor
// 디스패치가 문/블록 계층을 타고 내려가는지 검증).
TEST_F(OptimizerMockTest, FoldsBinaryExpressionNestedInsideIfBranchBlock) {
    auto cond = std::make_unique<BooleanExpression>(testTokens(TokenType::TRUE, "true", 1), true);
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto lit2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "2", 1), 2.0);
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 1), lit1.get(), lit2.get());
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), add.get());
    std::vector<Statement*> thenStmts{ printStmt.get() };
    auto thenBlock = std::make_unique<BlockStatement>(testTokens(1), thenStmts);
    auto ifStmt = std::make_unique<IfStatement>(testTokens(TokenType::IF, "if", 1), cond.get(), thenBlock.get());

    SyntaxNode* root = ifStmt.get();
    AddExpression* addRaw = add.get();
    tree.add(std::move(cond));
    tree.add(std::move(lit1));
    tree.add(std::move(lit2));
    tree.add(std::move(add));
    tree.add(std::move(printStmt));
    tree.add(std::move(thenBlock));
    tree.add(std::move(ifStmt));
    tree.setRoot(root);

    EXPECT_CALL(executor, evaluate(static_cast<Expression*>(addRaw))).Times(1).WillOnce(testing::Return(Value(3.0)));

    optimizer.optimize(tree);
}

// print 1 / 0;   -> evaluate()가 ExecutorError를 던지면 폴딩을 건너뛰고 원본을 그대로 둔다
// (컴파일 타임에 대신 오류를 내면 안 되고, Executor가 런타임에 오류를 내야 한다).
TEST_F(OptimizerTest, SkipsFoldingWhenEvaluateThrowsAndKeepsOriginalTree) {
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto lit0 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "0", 1), 0.0);
    auto divide = std::make_unique<DivideExpression>(testTokens(TokenType::SLASH, "/", 1), lit1.get(), lit0.get());
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), divide.get());

    SyntaxNode* root = printStmt.get();
    NumberExpression* lit1Raw = lit1.get();
    NumberExpression* lit0Raw = lit0.get();
    DivideExpression* divideRaw = divide.get();
    tree.add(std::move(lit1));
    tree.add(std::move(lit0));
    tree.add(std::move(divide));
    tree.add(std::move(printStmt));
    tree.setRoot(root);

    EXPECT_NO_THROW(optimizer.optimize(tree));

    // 0으로 나누기라 실제 Executor::evaluate()가 ExecutorError를 던지므로, 폴딩되지 않고
    // divide->left/right는 원래 리터럴 노드를 그대로 가리켜야 한다.
    EXPECT_EQ(lit1Raw, divideRaw->left);
    EXPECT_EQ(lit0Raw, divideRaw->right);
}
