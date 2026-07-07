#pragma once

#include <memory>
#include <vector>
#include <string>

#include "Token.h"

// Syntax tree
class SyntaxNode {
public:
    SyntaxNode(const std::vector<Token>& tokens) : tokens(tokens) {}
    virtual ~SyntaxNode() = default;

    virtual bool operator==(const SyntaxNode& op) const = 0;

    // [추가] checker가 에러 메시지에 줄 번호를 표기하기 위한 접근자.
    // ASSUMPTION: 노드 생성 시 tokens에 최소 1개 이상의 토큰이 들어있다고 가정.
    int getLine() const {
        return tokens.empty() ? -1 : tokens.front().line;
    }

private:
    std::vector<Token> tokens;
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

struct PrintStatement : public Statement {
    Expression* expr;

    PrintStatement(const std::vector<Token>& tokens, Expression* expr) : Statement(tokens), expr(expr) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const PrintStatement*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && *expr == *node->expr;
    }
};

struct NumberExpression : public Expression {
    double value;

    NumberExpression(const std::vector<Token>& tokens, double value) : Expression(tokens), value(value) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const NumberExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && value == node->value;
    }
};

struct BinaryExpression : public Expression {
    Expression* left;
    Expression* right;

    BinaryExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : Expression(tokens), left(left), right(right) {
    }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const BinaryExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && *left == *node->left && *right == *node->right;
    }
};

struct AddExpression : public BinaryExpression {
    AddExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {
    }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const AddExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct MultExpression : public BinaryExpression {
    MultExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {
    }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const MultExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

// ============================================================================
// 아래 3개(IdentifierExpression, VarDeclStatement, BlockStatement)는 Checker Unit의
// "변수 중복 선언 검사" / "선언 시 자기 참조 검사"에 반드시 필요해서 추가
// ============================================================================

struct IdentifierExpression : public Expression {
    std::string name;

    IdentifierExpression(const std::vector<Token>& tokens, std::string name)
        : Expression(tokens), name(std::move(name)) {
    }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const IdentifierExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && name == node->name;
    }
};

struct VarDeclStatement : public Statement {
    std::string name;
    Expression* initExpr;

    VarDeclStatement(const std::vector<Token>& tokens, std::string name, Expression* initExpr)
        : Statement(tokens), name(std::move(name)), initExpr(initExpr) {
    }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const VarDeclStatement*>(&op);
        if (!node)
            return false;
        if (!SyntaxNode::operator==(op) || name != node->name)
            return false;
        if ((initExpr == nullptr) != (node->initExpr == nullptr))
            return false;
        return initExpr == nullptr || *initExpr == *node->initExpr;
    }
};

struct BlockStatement : public Statement {
    std::vector<Statement*> statements;

    BlockStatement(const std::vector<Token>& tokens, std::vector<Statement*> statements)
        : Statement(tokens), statements(std::move(statements)) {
    }

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const BlockStatement*>(&op);
        if (!node)
            return false;
        if (!SyntaxNode::operator==(op) || statements.size() != node->statements.size())
            return false;
        for (size_t i = 0; i < statements.size(); ++i) {
            Statement* a = statements[i];
            Statement* b = node->statements[i];
            if ((a == nullptr) != (b == nullptr))
                return false;
            if (a != nullptr && !(*a == *b))
                return false;
        }
        return true;
    }
};

