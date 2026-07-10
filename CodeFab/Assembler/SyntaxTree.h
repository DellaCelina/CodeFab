#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <string>

#include "../Tokenizer/Token.h"

// SyntaxNodeVisitor가 각 구체 노드 타입을 참조하려면 정의보다 먼저 선언이 필요하다.
struct IdentifierExpression;
struct PrintStatement;
struct ExpressionStatement;
struct DeclareStatement;
struct BlockStatement;
struct IfStatement;
struct ForStatement;
struct NumberExpression;
struct StringExpression;
struct BooleanExpression;
struct AddExpression;
struct MultExpression;
struct SubExpression;
struct DivideExpression;
struct ModExpression;
struct AndExpression;
struct OrExpression;
struct EqualExpression;
struct NotEqualExpression;
struct LessExpression;
struct LessEqualExpression;
struct GreaterExpression;
struct GreaterEqualExpression;
struct AssignExpression;
struct NegativeExpression;
struct NotExpression;
struct CallExpression;
struct FieldAccessExpression;
struct ThisExpression;
struct SuperExpression;
struct ArrayExpression;
struct IndexExpression;
struct InstanceOfExpression;
struct FunctionDeclareStatement;
struct MethodDeclareStatement;
struct ReturnStatement;
struct ClassDeclareStatement;
struct ImportStatement;

// visit()가 값을 반환하지 않는 이유: Assembler 레이어가 Executor/Value.h에
// 의존할 수 없으므로, 방문자가 결과를 내부 상태(예: Executor::lastValue_)에
// 담아두고 호출부가 꺼내 쓰는 방식을 사용한다.
class SyntaxNodeVisitor {
public:
    virtual ~SyntaxNodeVisitor() = default;

    virtual void visit(IdentifierExpression& node) = 0;
    virtual void visit(PrintStatement& node) = 0;
    virtual void visit(ExpressionStatement& node) = 0;
    virtual void visit(DeclareStatement& node) = 0;
    virtual void visit(BlockStatement& node) = 0;
    virtual void visit(IfStatement& node) = 0;
    virtual void visit(ForStatement& node) = 0;
    virtual void visit(NumberExpression& node) = 0;
    virtual void visit(StringExpression& node) = 0;
    virtual void visit(BooleanExpression& node) = 0;
    virtual void visit(AddExpression& node) = 0;
    virtual void visit(MultExpression& node) = 0;
    virtual void visit(SubExpression& node) = 0;
    virtual void visit(DivideExpression& node) = 0;
    virtual void visit(ModExpression& node) = 0;
    virtual void visit(AndExpression& node) = 0;
    virtual void visit(OrExpression& node) = 0;
    virtual void visit(EqualExpression& node) = 0;
    virtual void visit(NotEqualExpression& node) = 0;
    virtual void visit(LessExpression& node) = 0;
    virtual void visit(LessEqualExpression& node) = 0;
    virtual void visit(GreaterExpression& node) = 0;
    virtual void visit(GreaterEqualExpression& node) = 0;
    virtual void visit(AssignExpression& node) = 0;
    virtual void visit(NegativeExpression& node) = 0;
    virtual void visit(NotExpression& node) = 0;
    virtual void visit(CallExpression& node) = 0;
    virtual void visit(FieldAccessExpression& node) = 0;
    virtual void visit(ThisExpression& node) = 0;
    virtual void visit(SuperExpression& node) = 0;
    virtual void visit(ArrayExpression& node) = 0;
    virtual void visit(IndexExpression& node) = 0;
    virtual void visit(InstanceOfExpression& node) = 0;
    virtual void visit(FunctionDeclareStatement& node) = 0;
    virtual void visit(MethodDeclareStatement& node) = 0;
    virtual void visit(ReturnStatement& node) = 0;
    virtual void visit(ClassDeclareStatement& node) = 0;
    virtual void visit(ImportStatement& node) = 0;
};

// 구문 트리 노드 기반 클래스.
class SyntaxNode {
public:
    SyntaxNode(const std::vector<Token>& tokens) : tokens(tokens) {}
    virtual ~SyntaxNode() = default;

    virtual bool operator==(const SyntaxNode& op) const = 0;

    virtual void accept(SyntaxNodeVisitor& visitor) = 0;

    // checker 등에서 에러 메시지에 줄 번호를 표기하기 위해 추가.
    int getLine() const {
        return tokens.empty() ? -1 : tokens.front().line;
    }

    // 여러 줄에 걸친 statement에서 중간 줄 breakpoint까지 잡으려면 이 메서드를 쓴다.
    bool containsLine(int line) const {
        for (const auto& token : tokens) {
            if (token.line == line)
                return true;
        }
        return false;
    }

private:
    const std::vector<Token> tokens;
};

inline bool SyntaxNode::operator==(const SyntaxNode& op) const {
    return tokens == op.tokens;
}

class SyntaxTree {
public:
    auto getRoot() {
        return roots.empty() ? nullptr : roots.front();
    }

    void setRoot(SyntaxNode* root) {
        roots = { root };
    }

    void addRoot(SyntaxNode* root) {
        roots.push_back(root);
    }

    const std::vector<SyntaxNode*>& getRoots() const {
        return roots;
    }

    void add(std::unique_ptr<SyntaxNode> node) {
        nodes.push_back(std::move(node));
    }

private:
    std::vector<std::unique_ptr<SyntaxNode>> nodes;
    std::vector<SyntaxNode*> roots;
};

struct Statement : public SyntaxNode {
    using SyntaxNode::SyntaxNode;
};
struct Expression : public SyntaxNode {
    using SyntaxNode::SyntaxNode;
};

struct IdentifierExpression : public Expression {
    const std::string name;

    // Checker가 채우는 스코프 거리 캐시(0 = 현재 스코프). 전역/import이면 nullopt.
    // operator==에 포함되지 않는 부가 정보라 mutable로 선언한다.
    mutable std::optional<int> depth;

    IdentifierExpression(const std::vector<Token>& tokens, const std::string& name) : Expression(tokens), name(name) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const IdentifierExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && name == node->name;
    }
};

struct PrintStatement : public Statement {
    Expression* const expr;

    PrintStatement(const std::vector<Token>& tokens, Expression* expr) : Statement(tokens), expr(expr) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const PrintStatement*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && expr->operator==(*node->expr);
    }
};

struct ExpressionStatement : public Statement {
    Expression* const expr;

    ExpressionStatement(const std::vector<Token>& tokens, Expression* expr) : Statement(tokens), expr(expr) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const ExpressionStatement*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && expr->operator==(*node->expr);
    }
};

struct DeclareStatement : public Statement {
    IdentifierExpression* const identifier;
    Expression* const expr;

    DeclareStatement(const std::vector<Token>& tokens, IdentifierExpression* identifier, Expression* expr)
        : Statement(tokens), identifier(identifier), expr(expr) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const DeclareStatement*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && identifier->operator==(*node->identifier) && expr->operator==(*node->expr);
    }
};

struct BlockStatement : public Statement {
    const std::vector<Statement*> statements;

    BlockStatement(const std::vector<Token>& tokens, const std::vector<Statement*>& statements)
        : Statement(tokens), statements(statements) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const BlockStatement*>(&op);
        if (!node)
            return false;
        if (statements.size() != node->statements.size())
            return false;
        for (size_t i = 0; i < statements.size(); i++) {
            if (!statements[i]->operator==(*node->statements[i]))
                return false;
        }
        return SyntaxNode::operator==(op);
    }
};

struct IfStatement : public Statement {
    Expression* const expr;
    Statement* const thenBranch;
    Statement* const elseBranch;

    IfStatement(const std::vector<Token>& tokens, Expression* expr, Statement* thenBranch, Statement* elseBranch = nullptr)
        : Statement(tokens), expr(expr), thenBranch(thenBranch), elseBranch(elseBranch) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const IfStatement*>(&op);
        if (!node)
            return false;
        if ((elseBranch == nullptr) != (node->elseBranch == nullptr))
            return false;
        if (elseBranch && !elseBranch->operator==(*node->elseBranch))
            return false;
        return SyntaxNode::operator==(op) && expr->operator==(*node->expr) && thenBranch->operator==(*node->thenBranch);
    }
};

struct ForStatement : public Statement {
    Statement* const init;
    Expression* const compare;
    Expression* const next;
    Statement* const loop;

    ForStatement(const std::vector<Token>& tokens, Statement* init, Expression* compare, Expression* next, Statement* loop)
        : Statement(tokens), init(init), compare(compare), next(next), loop(loop) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const ForStatement*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && init->operator==(*node->init) && compare->operator==(*node->compare)
            && next->operator==(*node->next) && loop->operator==(*node->loop);
    }
};

struct NumberExpression : public Expression {
    const double value;

    NumberExpression(const std::vector<Token>& tokens, double value) : Expression(tokens), value(value) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const NumberExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && value == node->value;
    }
};

struct StringExpression : public Expression {
    const std::string value;

    StringExpression(const std::vector<Token>& tokens, const std::string& value) : Expression(tokens), value(value) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const StringExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && value == node->value;
    }
};

struct BooleanExpression : public Expression {
    const bool value;

    BooleanExpression(const std::vector<Token>& tokens, bool value) : Expression(tokens), value(value) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const BooleanExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && value == node->value;
    }
};

// Optimizer가 상수 폴딩 결과로 left/right를 새 리터럴 노드로 교체할 수 있도록
// const가 아닌 포인터로 선언한다.
struct BinaryExpression : public Expression {
    Expression* left;
    Expression* right;

    BinaryExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : Expression(tokens), left(left), right(right) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const BinaryExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && left->operator==(*node->left) && right->operator==(*node->right);
    }
};

struct AddExpression : public BinaryExpression {
    AddExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const AddExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct MultExpression : public BinaryExpression {
    MultExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const MultExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct SubExpression : public BinaryExpression {
    SubExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const SubExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct DivideExpression : public BinaryExpression {
    DivideExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const DivideExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct ModExpression : public BinaryExpression {
    ModExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const ModExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct AndExpression : public BinaryExpression {
    AndExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const AndExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct OrExpression : public BinaryExpression {
    OrExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const OrExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct EqualExpression : public BinaryExpression {
    EqualExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const EqualExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct NotEqualExpression : public BinaryExpression {
    NotEqualExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const NotEqualExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct LessExpression : public BinaryExpression {
    LessExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const LessExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct LessEqualExpression : public BinaryExpression {
    LessEqualExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const LessEqualExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct GreaterExpression : public BinaryExpression {
    GreaterExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const GreaterExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct GreaterEqualExpression : public BinaryExpression {
    GreaterEqualExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const GreaterEqualExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

// 대입 대상(target)은 IdentifierExpression, FieldAccessExpression, IndexExpression 중 하나.
struct AssignExpression : public Expression {
    Expression* const target;
    Expression* const value;

    AssignExpression(const std::vector<Token>& tokens, Expression* target, Expression* value)
        : Expression(tokens), target(target), value(value) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const AssignExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && target->operator==(*node->target) && value->operator==(*node->value);
    }
};

// BinaryExpression과 동일한 이유로 operand를 non-const 포인터로 선언한다.
struct UnaryExpression : public Expression {
    Expression* operand;

    UnaryExpression(const std::vector<Token>& tokens, Expression* operand) : Expression(tokens), operand(operand) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const UnaryExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && operand->operator==(*node->operand);
    }
};

struct NegativeExpression : public UnaryExpression {
    NegativeExpression(const std::vector<Token>& tokens, Expression* operand) : UnaryExpression(tokens, operand) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const NegativeExpression*>(&op);
        if (!node)
            return false;
        return UnaryExpression::operator==(op);
    }
};

struct NotExpression : public UnaryExpression {
    NotExpression(const std::vector<Token>& tokens, Expression* operand) : UnaryExpression(tokens, operand) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const NotExpression*>(&op);
        if (!node)
            return false;
        return UnaryExpression::operator==(op);
    }
};

// 함수 호출과 클래스 인스턴스 생성(Robot())에 동일하게 쓰인다.
// Executor가 callee 타입(Function/Class)에 따라 실제 동작을 구분한다.
struct CallExpression : public Expression {
    Expression* const callee;
    const std::vector<Expression*> arguments;

    CallExpression(const std::vector<Token>& tokens, Expression* callee, const std::vector<Expression*>& arguments)
        : Expression(tokens), callee(callee), arguments(arguments) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const CallExpression*>(&op);
        if (!node)
            return false;
        if (arguments.size() != node->arguments.size())
            return false;
        for (size_t i = 0; i < arguments.size(); i++) {
            if (!arguments[i]->operator==(*node->arguments[i]))
                return false;
        }
        return SyntaxNode::operator==(op) && callee->operator==(*node->callee);
    }
};

// 필드 읽기, 메서드 호출, import 모듈 접근에 모두 쓰인다.
// 대입 좌변(r.name = 3)에서는 AssignExpression::target이 이 노드를 가리킨다.
struct FieldAccessExpression : public Expression {
    Expression* const object;
    const Token name;

    FieldAccessExpression(const std::vector<Token>& tokens, Expression* object, const Token& name)
        : Expression(tokens), object(object), name(name) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const FieldAccessExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && object->operator==(*node->object) && name == node->name;
    }
};

// This. 클래스 메서드 실행 중에만 유효하며, 호출 시 스코프에 "this"로 바인딩된다.
struct ThisExpression : public Expression {
    using Expression::Expression;

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        return dynamic_cast<const ThisExpression*>(&op) != nullptr && SyntaxNode::operator==(op);
    }
};

// Super. "Super.move(dist)"는 FieldAccessExpression(object=SuperExpression)으로
// 조립된다. Executor는 object가 SuperExpression인지를 보고 탐색 시작점을 superclass로 옮긴다.
struct SuperExpression : public Expression {
    using Expression::Expression;

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        return dynamic_cast<const SuperExpression*>(&op) != nullptr && SyntaxNode::operator==(op);
    }
};

// Array(3) 전용 문법. ARRAY가 예약어라 리터럴과 같은 층위에서 파싱된다.
struct ArrayExpression : public Expression {
    Expression* const sizeExpr;

    ArrayExpression(const std::vector<Token>& tokens, Expression* sizeExpr) : Expression(tokens), sizeExpr(sizeExpr) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const ArrayExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && sizeExpr->operator==(*node->sizeExpr);
    }
};

// arr[i] 읽기. 대입 좌변(arr[i] = 7)에서는 AssignExpression::target이 이 노드를 가리킨다.
struct IndexExpression : public Expression {
    Expression* const collection;
    Expression* const index;

    IndexExpression(const std::vector<Token>& tokens, Expression* collection, Expression* index)
        : Expression(tokens), collection(collection), index(index) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const IndexExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && collection->operator==(*node->collection) && index->operator==(*node->index);
    }
};

// a instanceof Robot. 우변은 항상 클래스 이름(식별자)이어야 하므로 별도
// Expression이 아니라 Token으로 받는다.
struct InstanceOfExpression : public Expression {
    Expression* const object;
    const Token className;

    InstanceOfExpression(const std::vector<Token>& tokens, Expression* object, const Token& className)
        : Expression(tokens), object(object), className(className) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const InstanceOfExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && object->operator==(*node->object) && className == node->className;
    }
};

// Func add(a, b) { ... }. 최상위 함수 선언 전용 노드다. params는 파라미터 이름
// 토큰 목록이다.
struct FunctionDeclareStatement : public Statement {
    const Token name;
    const std::vector<Token> params;
    const std::vector<Statement*> body;

    FunctionDeclareStatement(const std::vector<Token>& tokens, const Token& name, const std::vector<Token>& params,
        const std::vector<Statement*>& body)
        : Statement(tokens), name(name), params(params), body(body) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const FunctionDeclareStatement*>(&op);
        if (!node)
            return false;
        if (params != node->params)
            return false;
        if (body.size() != node->body.size())
            return false;
        for (size_t i = 0; i < body.size(); i++) {
            if (!body[i]->operator==(*node->body[i]))
                return false;
        }
        return SyntaxNode::operator==(op) && name == node->name;
    }
};

// 클래스 바디 안에서 FUNC 키워드 없이 선언되는 메서드 전용 노드.
// FunctionDeclareStatement와 필드 구조는 같지만 파싱 문맥이 달라 별도 타입으로 둔다.
// 생성자는 이름이 "init"인 MethodDeclareStatement다.
struct MethodDeclareStatement : public Statement {
    const Token name;
    const std::vector<Token> params;
    const std::vector<Statement*> body;

    MethodDeclareStatement(const std::vector<Token>& tokens, const Token& name, const std::vector<Token>& params,
        const std::vector<Statement*>& body)
        : Statement(tokens), name(name), params(params), body(body) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const MethodDeclareStatement*>(&op);
        if (!node)
            return false;
        if (params != node->params)
            return false;
        if (body.size() != node->body.size())
            return false;
        for (size_t i = 0; i < body.size(); i++) {
            if (!body[i]->operator==(*node->body[i]))
                return false;
        }
        return SyntaxNode::operator==(op) && name == node->name;
    }
};

// return; 또는 return <expr>;. value는 없을 수 있다(nullptr = 빈 return).
struct ReturnStatement : public Statement {
    Expression* const value;

    ReturnStatement(const std::vector<Token>& tokens, Expression* value = nullptr) : Statement(tokens), value(value) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const ReturnStatement*>(&op);
        if (!node)
            return false;
        if ((value == nullptr) != (node->value == nullptr))
            return false;
        if (value && !value->operator==(*node->value))
            return false;
        return SyntaxNode::operator==(op);
    }
};

// Class Robot { ... } 또는 Class SpeedRobot : Robot { ... }.
// superclass는 상속이 없으면 nullptr이다.
struct ClassDeclareStatement : public Statement {
    const Token name;
    const std::vector<MethodDeclareStatement*> methods;
    IdentifierExpression* const superclass;

    ClassDeclareStatement(const std::vector<Token>& tokens, const Token& name, const std::vector<MethodDeclareStatement*>& methods,
        IdentifierExpression* superclass = nullptr)
        : Statement(tokens), name(name), methods(methods), superclass(superclass) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const ClassDeclareStatement*>(&op);
        if (!node)
            return false;
        if (methods.size() != node->methods.size())
            return false;
        for (size_t i = 0; i < methods.size(); i++) {
            if (!methods[i]->operator==(*node->methods[i]))
                return false;
        }
        if ((superclass == nullptr) != (node->superclass == nullptr))
            return false;
        if (superclass && !superclass->operator==(*node->superclass))
            return false;
        return SyntaxNode::operator==(op) && name == node->name;
    }
};

// import "path" alias name;. declarations는 대상 파일의 최상위 선언 목록이다.
// 파싱은 Assembler가 이미 끝내므로 Checker/Executor는 파일 시스템에 접근할 필요가 없다.
struct ImportStatement : public Statement {
    const Token alias;
    const std::vector<Statement*> declarations;

    ImportStatement(const std::vector<Token>& tokens, const Token& alias, const std::vector<Statement*>& declarations)
        : Statement(tokens), alias(alias), declarations(declarations) {}

    void accept(SyntaxNodeVisitor& visitor) override { visitor.visit(*this); }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const ImportStatement*>(&op);
        if (!node)
            return false;
        if (declarations.size() != node->declarations.size())
            return false;
        for (size_t i = 0; i < declarations.size(); i++) {
            if (!declarations[i]->operator==(*node->declarations[i]))
                return false;
        }
        return SyntaxNode::operator==(op) && alias == node->alias;
    }
};
