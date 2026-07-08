#pragma once

#include <memory>
#include <vector>
#include <string>

#include "../Tokenizer/Token.h"

// Syntax tree
class SyntaxNode {
public:
    SyntaxNode(const std::vector<Token>& tokens) : tokens(tokens) {}
    virtual ~SyntaxNode() = default;

    virtual bool operator==(const SyntaxNode& op) const = 0;

    // checker 등에서 에러 메시지에 줄 번호를 표기하기 위해 추가.
    int getLine() const {
        return tokens.empty() ? -1 : tokens.front().line;
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
        return root;
    }

    void setRoot(SyntaxNode* root) {
        this->root = root;
    }

    void add(std::unique_ptr<SyntaxNode> node) {
        nodes.push_back(std::move(node));
    }

private:
    std::vector<std::unique_ptr<SyntaxNode>> nodes;
    SyntaxNode* root;
};

struct Statement : public SyntaxNode {
    using SyntaxNode::SyntaxNode;
};
struct Expression : public SyntaxNode {
    using SyntaxNode::SyntaxNode;
};

struct IdentifierExpression : public Expression {
    const std::string name;

    IdentifierExpression(const std::vector<Token>& tokens, const std::string& name) : Expression(tokens), name(name) {}

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

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const BooleanExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && value == node->value;
    }
};

struct BinaryExpression : public Expression {
    Expression* const left;
    Expression* const right;

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

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const DivideExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct EqualExpression : public BinaryExpression {
    EqualExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

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

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const GreaterEqualExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct AssignExpression : public Expression {
    IdentifierExpression* const identifier;
    Expression* const value;

    AssignExpression(const std::vector<Token>& tokens, IdentifierExpression* identifier, Expression* value)
        : Expression(tokens), identifier(identifier), value(value) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const AssignExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && identifier->operator==(*node->identifier) && value->operator==(*node->value);
    }
};

struct UnaryExpression : public Expression {
    Expression* const operand;

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

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const NegativeExpression*>(&op);
        if (!node)
            return false;
        return UnaryExpression::operator==(op);
    }
};

struct NotExpression : public UnaryExpression {
    NotExpression(const std::vector<Token>& tokens, Expression* operand) : UnaryExpression(tokens, operand) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const NotExpression*>(&op);
        if (!node)
            return false;
        return UnaryExpression::operator==(op);
    }
};
