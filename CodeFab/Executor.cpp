#include "Executor.h"

#include <stdexcept>
#include "ShellErrors.h"

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
        double right = evaluate(divide->right).asNumber();
        if (right == 0.0)
            throw RuntimeCodeFabError(0, "0으로 나눌 수 없습니다");
        return Value(evaluate(divide->left).asNumber() / right);
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

    // TODO(variables & assignment): register handlers for
    //   IdentifierExpression -> environment_.lookup(name); throw
    //     RuntimeCodeFabError(node->getLine(), "...") if undefined.
    //   DeclareStatement      -> environment_.define(identifier->name, evaluate(expr))
    //   AssignExpression      -> value = evaluate(value); if (!environment_.assign(...))
    //     throw RuntimeCodeFabError(...) for undefined target; return value.

    // TODO(block scope & control flow): register handlers for
    //   BlockStatement -> environment_.pushScope(); execute each statement;
    //     environment_.popScope() (use try/finally-style RAII or catch+rethrow
    //     so scope still pops if a statement throws).
    //   IfStatement    -> evaluate(expr).isTruthy() ? execute(thenBranch)
    //                      : (elseBranch ? execute(elseBranch) : void).
    //   ForStatement   -> execute(init-as-statement or evaluate as expr);
    //     while (evaluate(compare).isTruthy()) { execute(loop); evaluate(next); }

    expressionHandlers_[std::type_index(typeid(EqualExpression))] = [this](Expression* expr) {
        auto* equal = static_cast<EqualExpression*>(expr);
        return Value(evaluate(equal->left) == evaluate(equal->right));
    };

    expressionHandlers_[std::type_index(typeid(NotEqualExpression))] = [this](Expression* expr) {
        auto* notEqual = static_cast<NotEqualExpression*>(expr);
        return Value(!(evaluate(notEqual->left) == evaluate(notEqual->right)));
    };

    expressionHandlers_[std::type_index(typeid(LessEqualExpression))] = [this](Expression* expr) {
        auto* lessEqual = static_cast<LessEqualExpression*>(expr);
        return Value(evaluate(lessEqual->left).asNumber() <= evaluate(lessEqual->right).asNumber());
    };

    expressionHandlers_[std::type_index(typeid(GreaterEqualExpression))] = [this](Expression* expr) {
        auto* greaterEqual = static_cast<GreaterEqualExpression*>(expr);
        return Value(evaluate(greaterEqual->left).asNumber() >= evaluate(greaterEqual->right).asNumber());
    };

    expressionHandlers_[std::type_index(typeid(NotExpression))] = [this](Expression* expr) {
        auto* notExpr = static_cast<NotExpression*>(expr);
        return Value(!evaluate(notExpr->operand).isTruthy());
    };

    // TODO(remaining operators & runtime errors): register handlers for
    //   NotExpression -> done
    //   EqualExpression, NotEqualExpression, LessEqualExpression,
    //     GreaterEqualExpression -> done
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
