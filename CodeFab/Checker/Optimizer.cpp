#include "Optimizer.h"

#include <stdexcept>

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
    stmt->accept(*this);
}

Expression* Optimizer::foldExpression(Expression* expr) {
    if (expr == nullptr) {
        return expr;
    }
    expr->accept(*this);
    return lastFolded_;
}

void Optimizer::foldBinary(BinaryExpression& bin) {
    bin.left = foldExpression(bin.left);
    bin.right = foldExpression(bin.right);
    if (isLiteralExpression(bin.left) && isLiteralExpression(bin.right)) {
        try {
            Value v = executor_.evaluate(&bin);
            lastFolded_ = replaceWithLiteral(v, &bin);
            return;
        } catch (const ExecutorError&) {
            // 0으로 나누기 등 - 컴파일 타임에 대신 오류를 내면 안 되므로 원본을 그대로 둔다.
        }
    }
    lastFolded_ = &bin;
}

void Optimizer::foldUnary(UnaryExpression& un) {
    un.operand = foldExpression(un.operand);
    if (isLiteralExpression(un.operand)) {
        try {
            Value v = executor_.evaluate(&un);
            lastFolded_ = replaceWithLiteral(v, &un);
            return;
        } catch (const ExecutorError&) {
        }
    }
    lastFolded_ = &un;
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

// --- 리터럴/변수/this/super: 폴딩 대상이 아니다. 자기 자신을 그대로 반환한다. ---

void Optimizer::visit(NumberExpression& node) { lastFolded_ = &node; }
void Optimizer::visit(StringExpression& node) { lastFolded_ = &node; }
void Optimizer::visit(BooleanExpression& node) { lastFolded_ = &node; }
void Optimizer::visit(IdentifierExpression& node) { lastFolded_ = &node; }
void Optimizer::visit(ThisExpression& node) { lastFolded_ = &node; }
void Optimizer::visit(SuperExpression& node) { lastFolded_ = &node; }

// --- BinaryExpression 13종: 전부 foldBinary 공유. ---

void Optimizer::visit(AddExpression& node) { foldBinary(node); }
void Optimizer::visit(MultExpression& node) { foldBinary(node); }
void Optimizer::visit(SubExpression& node) { foldBinary(node); }
void Optimizer::visit(DivideExpression& node) { foldBinary(node); }
void Optimizer::visit(ModExpression& node) { foldBinary(node); }
void Optimizer::visit(AndExpression& node) { foldBinary(node); }
void Optimizer::visit(OrExpression& node) { foldBinary(node); }
void Optimizer::visit(EqualExpression& node) { foldBinary(node); }
void Optimizer::visit(NotEqualExpression& node) { foldBinary(node); }
void Optimizer::visit(LessExpression& node) { foldBinary(node); }
void Optimizer::visit(LessEqualExpression& node) { foldBinary(node); }
void Optimizer::visit(GreaterExpression& node) { foldBinary(node); }
void Optimizer::visit(GreaterEqualExpression& node) { foldBinary(node); }

// --- UnaryExpression 2종: foldUnary 공유. ---

void Optimizer::visit(NegativeExpression& node) { foldUnary(node); }
void Optimizer::visit(NotExpression& node) { foldUnary(node); }

// --- 그 외 Expression: target/callee/object 등 const 필드는 폴딩하지 않고,
//     남은 non-const 서브트리(인자, 인덱스 등)만 부작용으로 접는다. 자기 자신은
//     그대로 반환한다(대입 좌변/호출 대상 자체를 리터럴로 치환할 일은 없다). ---

void Optimizer::visit(AssignExpression& node) {
    // target은 대입 대상이라 폴딩하지 않는다.
    foldExpression(node.value);
    lastFolded_ = &node;
}

void Optimizer::visit(CallExpression& node) {
    // 호출 대상/인자는 CallExpression::callee/arguments가 여전히 const라 반환값을
    // 대입할 곳이 없다 - 자식 서브트리(중첩된 이항/단항 연산)만 부작용으로 접는다.
    foldExpression(node.callee);
    for (Expression* arg : node.arguments) {
        foldExpression(arg);
    }
    lastFolded_ = &node;
}

void Optimizer::visit(FieldAccessExpression& node) {
    foldExpression(node.object);
    lastFolded_ = &node;
}

void Optimizer::visit(ArrayExpression& node) {
    foldExpression(node.sizeExpr);
    lastFolded_ = &node;
}

void Optimizer::visit(IndexExpression& node) {
    foldExpression(node.collection);
    foldExpression(node.index);
    lastFolded_ = &node;
}

void Optimizer::visit(InstanceOfExpression& node) {
    foldExpression(node.object);
    lastFolded_ = &node;
}

// --- Statement: 자식 statement/expression을 재귀적으로 접는 부수효과만 있다. ---

void Optimizer::visit(BlockStatement& node) {
    for (Statement* s : node.statements) {
        foldStatement(s);
    }
}

void Optimizer::visit(DeclareStatement& node) {
    foldExpression(node.expr);
}

void Optimizer::visit(PrintStatement& node) {
    foldExpression(node.expr);
}

void Optimizer::visit(ExpressionStatement& node) {
    foldExpression(node.expr);
}

void Optimizer::visit(IfStatement& node) {
    foldExpression(node.expr);
    foldStatement(node.thenBranch);
    foldStatement(node.elseBranch);
}

void Optimizer::visit(ForStatement& node) {
    foldStatement(node.init);
    foldExpression(node.compare);
    foldExpression(node.next);
    foldStatement(node.loop);
}

void Optimizer::visit(FunctionDeclareStatement& node) {
    for (Statement* s : node.body) {
        foldStatement(s);
    }
}

void Optimizer::visit(MethodDeclareStatement&) {
    throw std::logic_error(
        "Optimizer::visit(MethodDeclareStatement&): 메서드는 accept()로 직접 방문되지 않는다 - "
        "visit(ClassDeclareStatement&)가 각 메서드의 body를 직접 접는다.");
}

void Optimizer::visit(ReturnStatement& node) {
    foldExpression(node.value);
}

void Optimizer::visit(ClassDeclareStatement& node) {
    for (MethodDeclareStatement* method : node.methods) {
        for (Statement* s : method->body) {
            foldStatement(s);
        }
    }
}

void Optimizer::visit(ImportStatement& node) {
    for (Statement* decl : node.declarations) {
        foldStatement(decl);
    }
}
