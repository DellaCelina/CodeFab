#pragma once

#include <memory>
#include <vector>

// Stand-in AST for developing/testing the Executor while the real
// Assembler's syntax_tree.h (still in progress upstream) isn't usable yet
// on this branch. Mirrors the node shapes (and the SyntaxTree ownership
// model) the real tree is expected to have, so swapping the Executor over
// to the real tree later is a mechanical include-path change, not a
// redesign.
struct SyntaxNode {
    virtual ~SyntaxNode() = default;
};

struct Expression : SyntaxNode {};

struct Statement : SyntaxNode {};

struct PrintStatement : Statement {
    Expression* expr;

    explicit PrintStatement(Expression* expr) : expr(expr) {}
};

struct NumberExpression : Expression {
    double value;

    explicit NumberExpression(double value) : value(value) {}
};

struct BinaryExpression : Expression {
    Expression* left;
    Expression* right;

    BinaryExpression(Expression* left, Expression* right) : left(left), right(right) {}
};

struct AddExpression : BinaryExpression {
    AddExpression(Expression* left, Expression* right) : BinaryExpression(left, right) {}
};

struct MultExpression : BinaryExpression {
    MultExpression(Expression* left, Expression* right) : BinaryExpression(left, right) {}
};

// Owns every node produced while assembling a program; `root` is a
// non-owning pointer into that pool, matching the real SyntaxTree's
// ownership model (see syntax_tree.h).
class SyntaxTree {
public:
    SyntaxNode* getRoot() const { return root; }

    void setRoot(SyntaxNode* root) { this->root = root; }

    void add(std::unique_ptr<SyntaxNode> node) { nodes.push_back(std::move(node)); }

private:
    std::vector<std::unique_ptr<SyntaxNode>> nodes;
    SyntaxNode* root = nullptr;
};
