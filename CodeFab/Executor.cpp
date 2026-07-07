#include "Executor.h"

#include <stdexcept>

#include "ShellErrors.h"

namespace {
// TODO: replace with the offending node's own line once
// SyntaxNode::getLine() (added in PR #5) lands on this branch.
constexpr int kUnknownLine = 0;
}  // namespace

Executor::Executor(std::ostream& out) : out_(out) {
    registerDefaultHandlers();
}

void Executor::registerDefaultHandlers() {
    statementHandlers_[std::type_index(typeid(PrintStatement))] = [this](Statement* stmt) {
        auto* print = static_cast<PrintStatement*>(stmt);
        out_ << evaluate(print->expr).toString() << std::endl;
    };

    expressionHandlers_[std::type_index(typeid(NumberExpression))] = [](Expression* expr) {
        return Value(static_cast<NumberExpression*>(expr)->value);
    };

    expressionHandlers_[std::type_index(typeid(StringExpression))] = [](Expression* expr) {
        return Value(static_cast<StringExpression*>(expr)->value);
    };

    expressionHandlers_[std::type_index(typeid(BooleanExpression))] = [](Expression* expr) {
        return Value(static_cast<BooleanExpression*>(expr)->value);
    };

    expressionHandlers_[std::type_index(typeid(AddExpression))] = [this](Expression* expr) {
        auto* add = static_cast<AddExpression*>(expr);
        Value left = evaluate(add->left);
        Value right = evaluate(add->right);
        if (left.isString() && right.isString()) {
            return Value(left.asString() + right.asString());
        }
        return Value(left.asNumber() + right.asNumber());
    };

    expressionHandlers_[std::type_index(typeid(SubExpression))] = [this](Expression* expr) {
        auto* sub = static_cast<SubExpression*>(expr);
        return Value(evaluate(sub->left).asNumber() - evaluate(sub->right).asNumber());
    };

    expressionHandlers_[std::type_index(typeid(MultExpression))] = [this](Expression* expr) {
        auto* mult = static_cast<MultExpression*>(expr);
        return Value(evaluate(mult->left).asNumber() * evaluate(mult->right).asNumber());
    };

    expressionHandlers_[std::type_index(typeid(DivideExpression))] = [this](Expression* expr) {
        auto* divide = static_cast<DivideExpression*>(expr);
        return Value(evaluate(divide->left).asNumber() / evaluate(divide->right).asNumber());
    };

    expressionHandlers_[std::type_index(typeid(NegativeExpression))] = [this](Expression* expr) {
        auto* negative = static_cast<NegativeExpression*>(expr);
        return Value(-evaluate(negative->operand).asNumber());
    };

    expressionHandlers_[std::type_index(typeid(LessExpression))] = [this](Expression* expr) {
        auto* less = static_cast<LessExpression*>(expr);
        return Value(evaluate(less->left).asNumber() < evaluate(less->right).asNumber());
    };

    expressionHandlers_[std::type_index(typeid(GreaterExpression))] = [this](Expression* expr) {
        auto* greater = static_cast<GreaterExpression*>(expr);
        return Value(evaluate(greater->left).asNumber() > evaluate(greater->right).asNumber());
    };

    expressionHandlers_[std::type_index(typeid(IdentifierExpression))] = [this](Expression* expr) {
        auto* identifier = static_cast<IdentifierExpression*>(expr);
        auto value = environment_.lookup(identifier->name);
        if (!value) {
            throw RuntimeCodeFabError(kUnknownLine, "'" + identifier->name + "' 변수가 정의되지 않았습니다.");
        }
        return *value;
    };

    expressionHandlers_[std::type_index(typeid(AssignExpression))] = [this](Expression* expr) {
        auto* assign = static_cast<AssignExpression*>(expr);
        Value value = evaluate(assign->value);
        if (!environment_.assign(assign->identifier->name, value)) {
            throw RuntimeCodeFabError(kUnknownLine, "'" + assign->identifier->name + "' 변수가 정의되지 않았습니다.");
        }
        return value;
    };

    statementHandlers_[std::type_index(typeid(DeclareStatement))] = [this](Statement* stmt) {
        auto* decl = static_cast<DeclareStatement*>(stmt);
        environment_.define(decl->identifier->name, evaluate(decl->expr));
    };

    statementHandlers_[std::type_index(typeid(ExpressionStatement))] = [this](Statement* stmt) {
        auto* exprStmt = static_cast<ExpressionStatement*>(stmt);
        evaluate(exprStmt->expr);
    };

    statementHandlers_[std::type_index(typeid(BlockStatement))] = [this](Statement* stmt) {
        auto* block = static_cast<BlockStatement*>(stmt);
        environment_.pushScope();
        try {
            for (Statement* inner : block->statements) {
                execute(inner);
            }
        } catch (...) {
            environment_.popScope();
            throw;
        }
        environment_.popScope();
    };

    // TODO(control flow): register handlers for
    //   IfStatement    -> evaluate(expr).isTruthy() ? execute(thenBranch)
    //                      : (elseBranch ? execute(elseBranch) : void).
    //   ForStatement   -> execute(init-as-statement or evaluate as expr);
    //     while (evaluate(compare).isTruthy()) { execute(loop); evaluate(next); }

    // TODO(remaining operators & runtime errors): register handlers for
    //   EqualExpression, NotEqualExpression, LessEqualExpression,
    //     GreaterEqualExpression (same shape as Less/GreaterExpression above)
    //   NotExpression -> Value(!evaluate(operand).isTruthy())
    //   DivideExpression: add divide-by-zero check, throw RuntimeCodeFabError
    //   Add/Sub/Mult/Divide/comparisons: wrap asNumber()/asString() mismatches
    //     (currently a raw std::bad_variant_access) into a clear
    //     RuntimeCodeFabError("피연산자는 반드시 숫자여야 합니다", node->getLine())
}

void Executor::execute(SyntaxTree& tree) {
    auto* root = dynamic_cast<Statement*>(tree.getRoot());
    if (!root) {
        throw std::logic_error("Executor::execute: tree root is not a Statement");
    }
    execute(root);
}

void Executor::execute(Statement* stmt) {
    auto it = statementHandlers_.find(std::type_index(typeid(*stmt)));
    if (it == statementHandlers_.end()) {
        throw std::logic_error("Executor::execute: no handler registered for this statement node");
    }
    it->second(stmt);
}

Value Executor::evaluate(Expression* expr) {
    auto it = expressionHandlers_.find(std::type_index(typeid(*expr)));
    if (it == expressionHandlers_.end()) {
        throw std::logic_error("Executor::evaluate: no handler registered for this expression node");
    }
    return it->second(expr);
}
