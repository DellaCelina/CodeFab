#pragma once

// Stand-in AST for developing/testing the Executor while the real
// Assembler's syntax_tree.h (still in progress upstream) isn't usable yet
// on this branch. Mirrors the node shapes PrintStatement/NumberExpression/
// AddExpression/MultExpression are expected to have, so swapping the
// Executor over to the real tree later is a mechanical include-path change,
// not a redesign.
struct Expression {
    virtual ~Expression() = default;
};

struct Statement {
    virtual ~Statement() = default;
};

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
