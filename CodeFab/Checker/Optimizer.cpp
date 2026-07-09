#include "Optimizer.h"

namespace {

bool isLiteralExpression(Expression* expr) {
    return dynamic_cast<NumberExpression*>(expr) != nullptr
        || dynamic_cast<BooleanExpression*>(expr) != nullptr
        || dynamic_cast<StringExpression*>(expr) != nullptr;
}

}  // namespace

void Optimizer::optimize(SyntaxTree& tree) {
    tree_ = &tree;
    foldStatement(dynamic_cast<Statement*>(tree.getRoot()));
    tree_ = nullptr;
}

void Optimizer::foldStatement(Statement* stmt) {
    if (stmt == nullptr) {
        return;
    }

    if (auto* block = dynamic_cast<BlockStatement*>(stmt)) {
        for (Statement* s : block->statements) {
            foldStatement(s);
        }
    }
    else if (auto* decl = dynamic_cast<DeclareStatement*>(stmt)) {
        foldExpression(decl->expr);
    }
    else if (auto* print = dynamic_cast<PrintStatement*>(stmt)) {
        foldExpression(print->expr);
    }
    else if (auto* exprStmt = dynamic_cast<ExpressionStatement*>(stmt)) {
        foldExpression(exprStmt->expr);
    }
    else if (auto* ifStmt = dynamic_cast<IfStatement*>(stmt)) {
        foldExpression(ifStmt->expr);
        foldStatement(ifStmt->thenBranch);
        foldStatement(ifStmt->elseBranch);
    }
    else if (auto* forStmt = dynamic_cast<ForStatement*>(stmt)) {
        foldStatement(forStmt->init);
        foldExpression(forStmt->compare);
        foldExpression(forStmt->next);
        foldStatement(forStmt->loop);
    }
    else if (auto* funcDecl = dynamic_cast<FunctionDeclareStatement*>(stmt)) {
        for (Statement* s : funcDecl->body) {
            foldStatement(s);
        }
    }
    else if (auto* classDecl = dynamic_cast<ClassDeclareStatement*>(stmt)) {
        for (MethodDeclareStatement* method : classDecl->methods) {
            for (Statement* s : method->body) {
                foldStatement(s);
            }
        }
    }
    else if (auto* ret = dynamic_cast<ReturnStatement*>(stmt)) {
        foldExpression(ret->value);
    }
    else if (auto* importStmt = dynamic_cast<ImportStatement*>(stmt)) {
        for (Statement* decl : importStmt->declarations) {
            foldStatement(decl);
        }
    }
}

Expression* Optimizer::foldExpression(Expression* expr) {
    if (expr == nullptr) {
        return expr;
    }

    if (auto* bin = dynamic_cast<BinaryExpression*>(expr)) {
        bin->left = foldExpression(bin->left);
        bin->right = foldExpression(bin->right);
        if (isLiteralExpression(bin->left) && isLiteralExpression(bin->right)) {
            try {
                Value v = executor_.evaluate(bin);
                return replaceWithLiteral(v, bin);
            } catch (const ExecutorError&) {
                // 0으로 나누기 등 - 컴파일 타임에 대신 오류를 내면 안 되므로 원본을 그대로 둔다.
            }
        }
        return bin;
    }
    if (auto* un = dynamic_cast<UnaryExpression*>(expr)) {
        un->operand = foldExpression(un->operand);
        if (isLiteralExpression(un->operand)) {
            try {
                Value v = executor_.evaluate(un);
                return replaceWithLiteral(v, un);
            } catch (const ExecutorError&) {
            }
        }
        return un;
    }
    if (auto* assign = dynamic_cast<AssignExpression*>(expr)) {
        foldExpression(assign->value); // target은 대입 대상이라 폴딩하지 않는다.
    }
    else if (auto* call = dynamic_cast<CallExpression*>(expr)) {
        // 호출 대상/인자는 CallExpression::callee/arguments가 여전히 const라 반환값을
        // 대입할 곳이 없다 - 자식 서브트리(중첩된 이항/단항 연산)만 부작용으로 접는다.
        foldExpression(call->callee);
        for (Expression* arg : call->arguments) {
            foldExpression(arg);
        }
    }
    else if (auto* field = dynamic_cast<FieldAccessExpression*>(expr)) {
        foldExpression(field->object);
    }
    else if (auto* idx = dynamic_cast<IndexExpression*>(expr)) {
        foldExpression(idx->collection);
        foldExpression(idx->index);
    }
    else if (auto* instOf = dynamic_cast<InstanceOfExpression*>(expr)) {
        foldExpression(instOf->object);
    }
    else if (auto* arr = dynamic_cast<ArrayExpression*>(expr)) {
        foldExpression(arr->sizeExpr);
    }
    // 리터럴/식별자/This/Super는 그 자체로 폴딩 대상이 아니다.
    return expr;
}

Expression* Optimizer::replaceWithLiteral(const Value& value, Expression* original) {
    int line = original->getLine();
    if (value.isNumber()) {
        auto node = std::make_unique<NumberExpression>(
            std::vector<Token>{ Token{ TokenType::NUMBER, std::to_string(value.asNumber()), line } }, value.asNumber());
        Expression* literal = node.get();
        tree_->add(std::move(node));
        return literal;
    }
    if (value.isBoolean()) {
        TokenType type = value.asBoolean() ? TokenType::TRUE : TokenType::FALSE;
        std::string origin = value.asBoolean() ? "true" : "false";
        auto node = std::make_unique<BooleanExpression>(
            std::vector<Token>{ Token{ type, origin, line } }, value.asBoolean());
        Expression* literal = node.get();
        tree_->add(std::move(node));
        return literal;
    }
    if (value.isString()) {
        auto node = std::make_unique<StringExpression>(
            std::vector<Token>{ Token{ TokenType::STRING, value.asString(), line } }, value.asString());
        Expression* literal = node.get();
        tree_->add(std::move(node));
        return literal;
    }
    // 산술/비교 연산 결과는 항상 Number/Boolean/String이므로 이론상 도달하지 않는다.
    return original;
}
