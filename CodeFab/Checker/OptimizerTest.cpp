#include <sstream>
#include <stdexcept>

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

// AddExpression/MultExpression/DivideExpression 외 나머지 10개 BinaryExpression
// 하위 타입은 전부 foldBinary()로만 위임하는 한 줄짜리 visit()라(Optimizer.cpp),
// 각 타입의 dispatch 자체가 실제로 호출되는지만 top-level에서 evaluate() 호출
// 여부로 확인한다(값의 정확성은 ExecutorTest류가 이미 검증).
template <typename BinaryOp>
static void ExpectTopLevelBinaryOpCallsEvaluateOnce() {
    SyntaxTree tree;
    MockExecuteInterface executor;
    Optimizer optimizer(executor);

    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto lit2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "2", 1), 2.0);
    auto op = std::make_unique<BinaryOp>(testTokens(1), lit1.get(), lit2.get());
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), op.get());

    SyntaxNode* root = printStmt.get();
    Expression* opRaw = op.get();
    tree.add(std::move(lit1));
    tree.add(std::move(lit2));
    tree.add(std::move(op));
    tree.add(std::move(printStmt));
    tree.setRoot(root);

    EXPECT_CALL(executor, evaluate(opRaw)).Times(1).WillOnce(testing::Return(Value(true)));

    optimizer.optimize(tree);
}

TEST(OptimizerBinaryDispatchTest, SubExpression_DispatchesToFoldBinary) {
    ExpectTopLevelBinaryOpCallsEvaluateOnce<SubExpression>();
}
TEST(OptimizerBinaryDispatchTest, ModExpression_DispatchesToFoldBinary) {
    ExpectTopLevelBinaryOpCallsEvaluateOnce<ModExpression>();
}
TEST(OptimizerBinaryDispatchTest, AndExpression_DispatchesToFoldBinary) {
    ExpectTopLevelBinaryOpCallsEvaluateOnce<AndExpression>();
}
TEST(OptimizerBinaryDispatchTest, OrExpression_DispatchesToFoldBinary) {
    ExpectTopLevelBinaryOpCallsEvaluateOnce<OrExpression>();
}
TEST(OptimizerBinaryDispatchTest, EqualExpression_DispatchesToFoldBinary) {
    ExpectTopLevelBinaryOpCallsEvaluateOnce<EqualExpression>();
}
TEST(OptimizerBinaryDispatchTest, NotEqualExpression_DispatchesToFoldBinary) {
    ExpectTopLevelBinaryOpCallsEvaluateOnce<NotEqualExpression>();
}
TEST(OptimizerBinaryDispatchTest, LessExpression_DispatchesToFoldBinary) {
    ExpectTopLevelBinaryOpCallsEvaluateOnce<LessExpression>();
}
TEST(OptimizerBinaryDispatchTest, LessEqualExpression_DispatchesToFoldBinary) {
    ExpectTopLevelBinaryOpCallsEvaluateOnce<LessEqualExpression>();
}
TEST(OptimizerBinaryDispatchTest, GreaterExpression_DispatchesToFoldBinary) {
    ExpectTopLevelBinaryOpCallsEvaluateOnce<GreaterExpression>();
}
TEST(OptimizerBinaryDispatchTest, GreaterEqualExpression_DispatchesToFoldBinary) {
    ExpectTopLevelBinaryOpCallsEvaluateOnce<GreaterEqualExpression>();
}

// print !true and true;   -> NotExpression(true)가 AndExpression::left(비-const)
// 자리에 있으므로 단항 폴딩으로 false 리터럴로 치환된다 - NotExpression의
// visit() dispatch와 replaceWithLiteral()의 Boolean 분기를 함께 확인한다.
TEST_F(OptimizerTest, FoldsNotExpressionIntoBooleanLiteralNestedInsideAndExpression) {
    auto litTrue1 = std::make_unique<BooleanExpression>(testTokens(TokenType::TRUE, "true", 1), true);
    auto litTrue2 = std::make_unique<BooleanExpression>(testTokens(TokenType::TRUE, "true", 1), true);
    auto notExpr = std::make_unique<NotExpression>(testTokens(TokenType::BANG, "!", 1), litTrue1.get());
    auto andExpr = std::make_unique<AndExpression>(testTokens(TokenType::AND, "and", 1), notExpr.get(), litTrue2.get());
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), andExpr.get());

    SyntaxNode* root = printStmt.get();
    AndExpression* andRaw = andExpr.get();
    tree.add(std::move(litTrue1));
    tree.add(std::move(litTrue2));
    tree.add(std::move(notExpr));
    tree.add(std::move(andExpr));
    tree.add(std::move(printStmt));
    tree.setRoot(root);

    optimizer.optimize(tree);

    auto* foldedLeft = dynamic_cast<BooleanExpression*>(andRaw->left);
    ASSERT_NE(nullptr, foldedLeft) << "!true가 리터럴로 치환돼야 합니다.";
    EXPECT_FALSE(foldedLeft->value);
}

// print ("foo" + "bar") + "!";   -> 문자열 연결도 리터럴 폴딩 대상이며, 결과가
// StringExpression으로 치환되는지(replaceWithLiteral()의 String 분기) 확인한다.
TEST_F(OptimizerTest, FoldsNestedStringConcatenationIntoStringLiteral) {
    auto litFoo = std::make_unique<StringExpression>(testTokens(TokenType::STRING, "foo", 1), "foo");
    auto litBar = std::make_unique<StringExpression>(testTokens(TokenType::STRING, "bar", 1), "bar");
    auto inner = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 1), litFoo.get(), litBar.get());
    auto litBang = std::make_unique<StringExpression>(testTokens(TokenType::STRING, "!", 1), "!");
    auto outer = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 1), inner.get(), litBang.get());
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), outer.get());

    SyntaxNode* root = printStmt.get();
    AddExpression* outerRaw = outer.get();
    tree.add(std::move(litFoo));
    tree.add(std::move(litBar));
    tree.add(std::move(inner));
    tree.add(std::move(litBang));
    tree.add(std::move(outer));
    tree.add(std::move(printStmt));
    tree.setRoot(root);

    optimizer.optimize(tree);

    auto* foldedLeft = dynamic_cast<StringExpression*>(outerRaw->left);
    ASSERT_NE(nullptr, foldedLeft) << "\"foo\" + \"bar\"가 리터럴로 치환돼야 합니다.";
    EXPECT_EQ("foobar", foldedLeft->value);
}

// print -true;   -> NegativeExpression의 리터럴 피연산자가 boolean이라 실제
// Executor::evaluate()가 ExecutorError를 던진다(단항 '-'는 숫자만 허용) -
// foldUnary()의 catch(ExecutorError) 분기와, 폴딩을 건너뛰고 원본 노드를 그대로
// 두는 폴백(lastFolded_ = &un;)을 함께 확인한다.
TEST_F(OptimizerTest, FoldUnary_WhenEvaluateThrowsExecutorError_SkipsFoldingAndKeepsOriginalNode) {
    auto litTrue = std::make_unique<BooleanExpression>(testTokens(TokenType::TRUE, "true", 1), true);
    auto neg = std::make_unique<NegativeExpression>(testTokens(TokenType::MINUS, "-", 1), litTrue.get());
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 1), neg.get(), lit1.get());
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), add.get());

    SyntaxNode* root = printStmt.get();
    AddExpression* addRaw = add.get();
    NegativeExpression* negRaw = neg.get();
    tree.add(std::move(litTrue));
    tree.add(std::move(neg));
    tree.add(std::move(lit1));
    tree.add(std::move(add));
    tree.add(std::move(printStmt));
    tree.setRoot(root);

    EXPECT_NO_THROW(optimizer.optimize(tree));

    EXPECT_EQ(negRaw, addRaw->left) << "-true는 폴딩되지 않고 원래 노드를 그대로 가리켜야 합니다.";
}

// Func square() { return 1 + 2; }   -> FunctionDeclareStatement::body 안의
// ReturnStatement::value도 재귀적으로 폴딩 대상이 되는지(evaluate() 호출 여부로)
// 확인한다.
TEST_F(OptimizerMockTest, FunctionDeclareStatement_FoldsLiteralExpressionInReturnValue) {
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto lit2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "2", 1), 2.0);
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 1), lit1.get(), lit2.get());
    auto returnStmt = std::make_unique<ReturnStatement>(testTokens(TokenType::RETURN, "return", 1), add.get());
    std::vector<Statement*> body{ returnStmt.get() };
    auto funcDecl = std::make_unique<FunctionDeclareStatement>(
        testTokens(TokenType::FUNC, "Func", 1), Token{ TokenType::IDENTIFIER, "square", 1 }, std::vector<Token>{}, body);

    SyntaxNode* root = funcDecl.get();
    Expression* addRaw = add.get();
    tree.add(std::move(lit1));
    tree.add(std::move(lit2));
    tree.add(std::move(add));
    tree.add(std::move(returnStmt));
    tree.add(std::move(funcDecl));
    tree.setRoot(root);

    EXPECT_CALL(executor, evaluate(addRaw)).Times(1).WillOnce(testing::Return(Value(3.0)));

    optimizer.optimize(tree);
}

// Func noop() { return; }   -> value가 없는 return은 foldExpression(nullptr)로
// 이어지고, 아무 것도 접지 않은 채(evaluate() 호출 없이) 조용히 끝나야 한다.
TEST_F(OptimizerMockTest, ReturnStatement_WithNoValue_DoesNotCallEvaluate) {
    auto returnStmt = std::make_unique<ReturnStatement>(testTokens(TokenType::RETURN, "return", 1));
    std::vector<Statement*> body{ returnStmt.get() };
    auto funcDecl = std::make_unique<FunctionDeclareStatement>(
        testTokens(TokenType::FUNC, "Func", 1), Token{ TokenType::IDENTIFIER, "noop", 1 }, std::vector<Token>{}, body);

    SyntaxNode* root = funcDecl.get();
    tree.add(std::move(returnStmt));
    tree.add(std::move(funcDecl));
    tree.setRoot(root);

    EXPECT_CALL(executor, evaluate(testing::_)).Times(0);

    EXPECT_NO_THROW(optimizer.optimize(tree));
}

// for (var i = 0; ; ) { print 1; }   -> compare/next가 없는(nullptr) for문도
// 안전하게 처리돼야 한다 - foldExpression(nullptr)의 null 가드(ForStatement::compare/
// next가 nullable)와 visit(ForStatement&) dispatch를 함께 확인한다.
TEST_F(OptimizerMockTest, ForStatement_WithNullCompareAndNext_DoesNotThrow) {
    auto identI = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "i", 1), "i");
    auto lit0 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "0", 1), 0.0);
    auto initDecl = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 1), identI.get(), lit0.get());
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), lit1.get());
    std::vector<Statement*> loopStmts{ printStmt.get() };
    auto loopBlock = std::make_unique<BlockStatement>(testTokens(1), loopStmts);
    auto forStmt = std::make_unique<ForStatement>(
        testTokens(TokenType::FOR, "for", 1), initDecl.get(), nullptr, nullptr, loopBlock.get());

    SyntaxNode* root = forStmt.get();
    tree.add(std::move(identI));
    tree.add(std::move(lit0));
    tree.add(std::move(initDecl));
    tree.add(std::move(lit1));
    tree.add(std::move(printStmt));
    tree.add(std::move(loopBlock));
    tree.add(std::move(forStmt));
    tree.setRoot(root);

    EXPECT_NO_THROW(optimizer.optimize(tree));
}

// import "math.cf" alias math;   // math.cf: var a = 1 + 2;
// -> ImportStatement::declarations 안의 선언도 재귀적으로 폴딩 대상이 되는지
// (evaluate() 호출 여부로) 확인한다.
TEST_F(OptimizerMockTest, ImportStatement_FoldsLiteralExpressionInDeclarations) {
    auto identA = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "a", 1), "a");
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto lit2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "2", 1), 2.0);
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 1), lit1.get(), lit2.get());
    auto declareA = std::make_unique<DeclareStatement>(testTokens(TokenType::VAR, "var", 1), identA.get(), add.get());
    std::vector<Statement*> declarations{ declareA.get() };
    auto importStmt = std::make_unique<ImportStatement>(
        testTokens(TokenType::IMPORT, "import", 1), Token{ TokenType::IDENTIFIER, "math", 1 }, declarations);

    SyntaxNode* root = importStmt.get();
    Expression* addRaw = add.get();
    tree.add(std::move(identA));
    tree.add(std::move(lit1));
    tree.add(std::move(lit2));
    tree.add(std::move(add));
    tree.add(std::move(declareA));
    tree.add(std::move(importStmt));
    tree.setRoot(root);

    EXPECT_CALL(executor, evaluate(addRaw)).Times(1).WillOnce(testing::Return(Value(3.0)));

    optimizer.optimize(tree);
}

// MethodDeclareStatement는 클래스 바디 전용이라 accept()로 직접 방문되지
// 않는다(visit(ClassDeclareStatement&)가 각 메서드의 body를 직접 접기 때문 -
// Optimizer.h 주석 참고). 그래서 이 방어적 분기는 정상 실행 경로로는 절대
// 도달하지 않고, visit()가 public이라는 사실을 이용해 직접 호출해야만 검증할
// 수 있다.
// print Array(1 + 2);   -> ArrayExpression::sizeExpr도 재귀적으로 폴딩 대상이
// 되는지(evaluate() 호출 여부로) 확인한다.
TEST_F(OptimizerMockTest, ArrayExpression_FoldsLiteralExpressionInSize) {
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto lit2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "2", 1), 2.0);
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 1), lit1.get(), lit2.get());
    auto arrayExpr = std::make_unique<ArrayExpression>(testTokens(TokenType::ARRAY, "Array", 1), add.get());
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), arrayExpr.get());

    SyntaxNode* root = printStmt.get();
    Expression* addRaw = add.get();
    tree.add(std::move(lit1));
    tree.add(std::move(lit2));
    tree.add(std::move(add));
    tree.add(std::move(arrayExpr));
    tree.add(std::move(printStmt));
    tree.setRoot(root);

    EXPECT_CALL(executor, evaluate(addRaw)).Times(1).WillOnce(testing::Return(Value(3.0)));

    optimizer.optimize(tree);
}

// print arr[1 + 2];   -> IndexExpression::collection/index 둘 다 재귀적으로
// 폴딩 대상이 되는지 확인한다. collection(식별자, 리터럴이 아님)은 evaluate()를
// 부르지 않고, index(1+2, 리터럴 두 개)는 evaluate()가 정확히 한 번 호출된다.
TEST_F(OptimizerMockTest, IndexExpression_FoldsLiteralExpressionInIndexButNotVariableCollection) {
    auto arrIdent = std::make_unique<IdentifierExpression>(testTokens(TokenType::IDENTIFIER, "arr", 1), "arr");
    auto lit1 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "1", 1), 1.0);
    auto lit2 = std::make_unique<NumberExpression>(testTokens(TokenType::NUMBER, "2", 1), 2.0);
    auto add = std::make_unique<AddExpression>(testTokens(TokenType::PLUS, "+", 1), lit1.get(), lit2.get());
    auto indexExpr = std::make_unique<IndexExpression>(testTokens(TokenType::LEFT_BRACKET, "[", 1), arrIdent.get(), add.get());
    auto printStmt = std::make_unique<PrintStatement>(testTokens(TokenType::PRINT, "print", 1), indexExpr.get());

    SyntaxNode* root = printStmt.get();
    Expression* addRaw = add.get();
    tree.add(std::move(arrIdent));
    tree.add(std::move(lit1));
    tree.add(std::move(lit2));
    tree.add(std::move(add));
    tree.add(std::move(indexExpr));
    tree.add(std::move(printStmt));
    tree.setRoot(root);

    EXPECT_CALL(executor, evaluate(addRaw)).Times(1).WillOnce(testing::Return(Value(3.0)));

    optimizer.optimize(tree);
}

TEST(OptimizerDirectVisitTest, MethodDeclareStatement_DirectVisitThrowsLogicErrorSinceNeverDispatchedByAccept) {
    std::ostringstream out;
    Executor executor(out);
    Optimizer optimizer(executor);

    MethodDeclareStatement method({}, Token{ TokenType::IDENTIFIER, "move", 1 }, {}, {});

    EXPECT_THROW(optimizer.visit(method), std::logic_error);
}
