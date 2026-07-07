#include "Executor.h"

#include <stdexcept>

namespace {

ExecutorError undefinedVariableError(const std::string& name) {
    return ExecutorError("'{}' 변수가 정의되지 않았습니다.", name);
}

// Pushes a new scope on construction and guarantees it's popped when the
// block ends, whether that's normal control flow or an exception unwinding
// through it (e.g. a statement inside the block throwing).
class ScopeGuard {
public:
    explicit ScopeGuard(Environment& environment) : environment_(environment) {
        environment_.pushScope();
    }

    ~ScopeGuard() {
        environment_.popScope();
    }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

private:
    Environment& environment_;
};
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
        if (left.isString() && right.isString())
            return Value(left.asString() + right.asString());
        if (left.isNumber() && right.isNumber())
            return Value(left.asNumber() + right.asNumber());
        throw ExecutorError("타입 오류: {} + {}", left.typeName(), right.typeName());
    };

    expressionHandlers_[std::type_index(typeid(SubExpression))] = [this](Expression* expr) {
        auto* sub = static_cast<SubExpression*>(expr);
        Value left = evaluate(sub->left);
        Value right = evaluate(sub->right);
        requireNumberOperands(left, right, "-");
        return Value(left.asNumber() - right.asNumber());
    };

    expressionHandlers_[std::type_index(typeid(MultExpression))] = [this](Expression* expr) {
        auto* mult = static_cast<MultExpression*>(expr);
        Value left = evaluate(mult->left);
        Value right = evaluate(mult->right);
        requireNumberOperands(left, right, "*");
        return Value(left.asNumber() * right.asNumber());
    };

    expressionHandlers_[std::type_index(typeid(DivideExpression))] = [this](Expression* expr) {
        auto* divide = static_cast<DivideExpression*>(expr);
        Value left = evaluate(divide->left);
        Value right = evaluate(divide->right);
        requireNumberOperands(left, right, "/");
        if (right.asNumber() == 0.0)
            throw ExecutorError("0으로 나눌 수 없습니다");
        return Value(left.asNumber() / right.asNumber());
    };

    expressionHandlers_[std::type_index(typeid(NegativeExpression))] = [this](Expression* expr) {
        auto* negative = static_cast<NegativeExpression*>(expr);
        Value operand = evaluate(negative->operand);
        if (!operand.isNumber())
            throw ExecutorError("타입 오류: -{}", operand.typeName());
        return Value(-operand.asNumber());
    };

    expressionHandlers_[std::type_index(typeid(LessExpression))] = [this](Expression* expr) {
        auto* less = static_cast<LessExpression*>(expr);
        Value left = evaluate(less->left);
        Value right = evaluate(less->right);
        requireNumberOperands(left, right, "<");
        return Value(left.asNumber() < right.asNumber());
    };

    expressionHandlers_[std::type_index(typeid(GreaterExpression))] = [this](Expression* expr) {
        auto* greater = static_cast<GreaterExpression*>(expr);
        Value left = evaluate(greater->left);
        Value right = evaluate(greater->right);
        requireNumberOperands(left, right, ">");
        return Value(left.asNumber() > right.asNumber());
    };

    expressionHandlers_[std::type_index(typeid(IdentifierExpression))] = [this](Expression* expr) {
        auto* identifier = static_cast<IdentifierExpression*>(expr);
        auto value = environment_.lookup(identifier->name);
        if (!value) {
            throw undefinedVariableError(identifier->name);
        }
        return *value;
    };

    expressionHandlers_[std::type_index(typeid(AssignExpression))] = [this](Expression* expr) {
        auto* assign = static_cast<AssignExpression*>(expr);
        Value value = evaluate(assign->value);
        if (!environment_.assign(assign->identifier->name, value)) {
            throw undefinedVariableError(assign->identifier->name);
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
        ScopeGuard guard(environment_);
        for (Statement* inner : block->statements) {
            execute(inner);
        }
    };

    statementHandlers_[std::type_index(typeid(IfStatement))] = [this](Statement* stmt) {
        auto* ifStmt = static_cast<IfStatement*>(stmt);
        if (evaluate(ifStmt->expr).isTruthy()) {
            execute(ifStmt->thenBranch);
        } else if (ifStmt->elseBranch) {
            execute(ifStmt->elseBranch);
        }
    };

    statementHandlers_[std::type_index(typeid(ForStatement))] = [this](Statement* stmt) {
        auto* forStmt = static_cast<ForStatement*>(stmt);
        evaluate(forStmt->init);
        while (evaluate(forStmt->compare).isTruthy()) {
            execute(forStmt->loop);
            evaluate(forStmt->next);
        }
    };

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
        Value left = evaluate(lessEqual->left);
        Value right = evaluate(lessEqual->right);
        requireNumberOperands(left, right, "<=");
        return Value(left.asNumber() <= right.asNumber());
    };

    expressionHandlers_[std::type_index(typeid(GreaterEqualExpression))] = [this](Expression* expr) {
        auto* greaterEqual = static_cast<GreaterEqualExpression*>(expr);
        Value left = evaluate(greaterEqual->left);
        Value right = evaluate(greaterEqual->right);
        requireNumberOperands(left, right, ">=");
        return Value(left.asNumber() >= right.asNumber());
    };

    expressionHandlers_[std::type_index(typeid(NotExpression))] = [this](Expression* expr) {
        auto* notExpr = static_cast<NotExpression*>(expr);
        return Value(!evaluate(notExpr->operand).isTruthy());
    };
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

void Executor::requireNumberOperands(const Value& left, const Value& right, const char* op) const {
    if (!left.isNumber() || !right.isNumber())
        throw ExecutorError("타입 오류: {} {} {}", left.typeName(), op, right.typeName());
}

Value Executor::evaluate(Expression* expr) {
    auto it = expressionHandlers_.find(std::type_index(typeid(*expr)));
    if (it == expressionHandlers_.end()) {
        throw std::logic_error("Executor::evaluate: no handler registered for this expression node");
    }
    return it->second(expr);
}
