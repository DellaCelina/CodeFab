#pragma once

#include <memory>
#include <vector>
#include <string>

// Temporal Token
enum TokenType {
    NUMBER,
};

struct Token {
    TokenType type;
    std::string orign;
    int line;
};

// Syntax tree
class SyntaxNode {
public:
    SyntaxNode(const std::vector<Token>& tokens) : tokens(tokens) {}
    virtual ~SyntaxNode() = default;

    int getLine() const {
        return tokens.empty() ? -1 : tokens.front().line;
    }

private:
    std::vector<Token> tokens;
};

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
    SyntaxNode* root = nullptr; 
};

struct Statement : public SyntaxNode {
    using SyntaxNode::SyntaxNode;
};
struct Expression : public SyntaxNode {
    using SyntaxNode::SyntaxNode;
};

struct PrintStatement : public Statement {
    using Statement::Statement;
    Expression* expr = nullptr;
};

struct BinaryExpression : public Expression {
    using Expression::Expression;
    Expression* left = nullptr;
    Expression* right = nullptr;
};

struct AddExpression : public BinaryExpression {
    using BinaryExpression::BinaryExpression;
};
struct MultExpression : public BinaryExpression {
    using BinaryExpression::BinaryExpression;
};


struct LiteralExpression : public Expression {
    using Expression::Expression;
    int value = 0;
};


struct IdentifierExpression : public Expression {
    using Expression::Expression;
    std::string name;
};


struct VarDeclStatement : public Statement {
    using Statement::Statement;
    std::string name;
    Expression* initExpr = nullptr;
};

struct BlockStatement : public Statement {
    using Statement::Statement;
    std::vector<Statement*> statements;
};
