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
    SyntaxNode* root;
};

struct Statement : public SyntaxNode {};
struct Expression : public SyntaxNode {};

struct PrintStatement : public Statement {
    Expression* expr;
};

struct BinaryExpression : public Expression {
    Expression* left;
    Expression* right;
};

struct AddExpression : public BinaryExpression {};
struct MultExpression : public BinaryExpression {};
