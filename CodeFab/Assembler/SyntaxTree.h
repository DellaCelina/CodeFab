#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <string>

#include "../Tokenizer/Token.h"

// Syntax tree
class SyntaxNode {
public:
    SyntaxNode(const std::vector<Token>& tokens) : tokens(tokens) {}
    virtual ~SyntaxNode() = default;

    virtual bool operator==(const SyntaxNode& op) const = 0;

    // checker 등에서 에러 메시지에 줄 번호를 표기하기 위해 추가.
    int getLine() const {
        return tokens.empty() ? -1 : tokens.front().line;
    }

    // 디버그 모드의 breakpoint 매칭용(Implement.md의 Shell 담당자 안내 참고): 이
    // 노드가 소비한 토큰들 중 하나라도 주어진 줄에 있으면 true. getLine()은 첫
    // 토큰의 줄만 보므로, 여러 줄에 걸친 statement(예: 여러 줄짜리 if 조건문)에서
    // breakpoint가 중간 줄에 찍힌 경우까지 잡아내려면 이 메서드를 쓴다.
    bool containsLine(int line) const {
        for (const auto& token : tokens) {
            if (token.line == line)
                return true;
        }
        return false;
    }

private:
    const std::vector<Token> tokens;
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

struct IdentifierExpression : public Expression {
    const std::string name;

    // 정적 바인딩(실행전 최적화) 결과 캐시. Checker의 Resolver가 이 식별자가 몇
    // 단계 위 스코프에서 선언되었는지 계산해 채워 넣는다(0 = 현재 스코프). 로컬
    // 스코프 어디에서도 못 찾으면(전역이거나 import 모듈 이름) nullopt로 남는다.
    // 노드의 "구문적 동일성"(operator==)에는 포함되지 않는 부가 정보이므로
    // mutable로 둔다 - Architecture.md §2.2, §6.1 참고.
    mutable std::optional<int> depth;

    IdentifierExpression(const std::vector<Token>& tokens, const std::string& name) : Expression(tokens), name(name) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const IdentifierExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && name == node->name;
    }
};

struct PrintStatement : public Statement {
    Expression* const expr;

    PrintStatement(const std::vector<Token>& tokens, Expression* expr) : Statement(tokens), expr(expr) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const PrintStatement*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && expr->operator==(*node->expr);
    }
};

struct ExpressionStatement : public Statement {
    Expression* const expr;

    ExpressionStatement(const std::vector<Token>& tokens, Expression* expr) : Statement(tokens), expr(expr) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const ExpressionStatement*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && expr->operator==(*node->expr);
    }
};

struct DeclareStatement : public Statement {
    IdentifierExpression* const identifier;
    Expression* const expr;

    DeclareStatement(const std::vector<Token>& tokens, IdentifierExpression* identifier, Expression* expr)
        : Statement(tokens), identifier(identifier), expr(expr) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const DeclareStatement*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && identifier->operator==(*node->identifier) && expr->operator==(*node->expr);
    }
};

struct BlockStatement : public Statement {
    const std::vector<Statement*> statements;

    BlockStatement(const std::vector<Token>& tokens, const std::vector<Statement*>& statements)
        : Statement(tokens), statements(statements) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const BlockStatement*>(&op);
        if (!node)
            return false;
        if (statements.size() != node->statements.size())
            return false;
        for (size_t i = 0; i < statements.size(); i++) {
            if (!statements[i]->operator==(*node->statements[i]))
                return false;
        }
        return SyntaxNode::operator==(op);
    }
};

struct IfStatement : public Statement {
    Expression* const expr;
    Statement* const thenBranch;
    Statement* const elseBranch;

    IfStatement(const std::vector<Token>& tokens, Expression* expr, Statement* thenBranch, Statement* elseBranch = nullptr)
        : Statement(tokens), expr(expr), thenBranch(thenBranch), elseBranch(elseBranch) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const IfStatement*>(&op);
        if (!node)
            return false;
        if ((elseBranch == nullptr) != (node->elseBranch == nullptr))
            return false;
        if (elseBranch && !elseBranch->operator==(*node->elseBranch))
            return false;
        return SyntaxNode::operator==(op) && expr->operator==(*node->expr) && thenBranch->operator==(*node->thenBranch);
    }
};

struct ForStatement : public Statement {
    Statement* const init;
    Expression* const compare;
    Expression* const next;
    Statement* const loop;

    ForStatement(const std::vector<Token>& tokens, Statement* init, Expression* compare, Expression* next, Statement* loop)
        : Statement(tokens), init(init), compare(compare), next(next), loop(loop) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const ForStatement*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && init->operator==(*node->init) && compare->operator==(*node->compare)
            && next->operator==(*node->next) && loop->operator==(*node->loop);
    }
};

struct NumberExpression : public Expression {
    const double value;

    NumberExpression(const std::vector<Token>& tokens, double value) : Expression(tokens), value(value) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const NumberExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && value == node->value;
    }
};

struct StringExpression : public Expression {
    const std::string value;

    StringExpression(const std::vector<Token>& tokens, const std::string& value) : Expression(tokens), value(value) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const StringExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && value == node->value;
    }
};

struct BooleanExpression : public Expression {
    const bool value;

    BooleanExpression(const std::vector<Token>& tokens, bool value) : Expression(tokens), value(value) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const BooleanExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && value == node->value;
    }
};

struct BinaryExpression : public Expression {
    Expression* const left;
    Expression* const right;

    BinaryExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : Expression(tokens), left(left), right(right) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const BinaryExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && left->operator==(*node->left) && right->operator==(*node->right);
    }
};

struct AddExpression : public BinaryExpression {
    AddExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const AddExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct MultExpression : public BinaryExpression {
    MultExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const MultExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct SubExpression : public BinaryExpression {
    SubExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const SubExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct DivideExpression : public BinaryExpression {
    DivideExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const DivideExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct EqualExpression : public BinaryExpression {
    EqualExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const EqualExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct NotEqualExpression : public BinaryExpression {
    NotEqualExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const NotEqualExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct LessExpression : public BinaryExpression {
    LessExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const LessExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct LessEqualExpression : public BinaryExpression {
    LessEqualExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const LessEqualExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct GreaterExpression : public BinaryExpression {
    GreaterExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const GreaterExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

struct GreaterEqualExpression : public BinaryExpression {
    GreaterEqualExpression(const std::vector<Token>& tokens, Expression* left, Expression* right)
        : BinaryExpression(tokens, left, right) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const GreaterEqualExpression*>(&op);
        if (!node)
            return false;
        return BinaryExpression::operator==(op);
    }
};

// 대입 대상(target)은 IdentifierExpression(a = 3), FieldAccessExpression(r.x = 3,
// 3일차 확장), IndexExpression(arr[i] = 3, 3일차 확장) 중 하나가 될 수 있다 -
// Architecture.md §2.2 "AssignExpression 대상 일반화" 참고. 지금 Assembler는
// IdentifierExpression만 만들어 넣지만, 필드/배열 대입 파싱이 추가되면 target
// 필드 타입을 바꿀 필요 없이 그대로 확장된다.
struct AssignExpression : public Expression {
    Expression* const target;
    Expression* const value;

    AssignExpression(const std::vector<Token>& tokens, Expression* target, Expression* value)
        : Expression(tokens), target(target), value(value) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const AssignExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && target->operator==(*node->target) && value->operator==(*node->value);
    }
};

struct UnaryExpression : public Expression {
    Expression* const operand;

    UnaryExpression(const std::vector<Token>& tokens, Expression* operand) : Expression(tokens), operand(operand) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const UnaryExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && operand->operator==(*node->operand);
    }
};

struct NegativeExpression : public UnaryExpression {
    NegativeExpression(const std::vector<Token>& tokens, Expression* operand) : UnaryExpression(tokens, operand) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const NegativeExpression*>(&op);
        if (!node)
            return false;
        return UnaryExpression::operator==(op);
    }
};

struct NotExpression : public UnaryExpression {
    NotExpression(const std::vector<Token>& tokens, Expression* operand) : UnaryExpression(tokens, operand) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const NotExpression*>(&op);
        if (!node)
            return false;
        return UnaryExpression::operator==(op);
    }
};

// ============================================================================
// 3일차 확장 노드 (function / class / array / import / instanceof)
//
// 이 노드들은 Architecture.md에서 설계된 AST 계약이다. 아직 Assembler는 이
// 노드들을 만들어내지 않고(문법 파싱 미구현), Checker/Executor도 아직 이 노드들을
// 처리하는 분기를 갖고 있지 않다 - 각자 Implement.md의 안내를 따라 채워 넣으면
// 된다. 노드 자체의 필드/생성자 시그니처는 세 모듈이 공통으로 합의한 것이므로
// 여기서 바꾸지 말고, 다른 이름/구조가 필요하면 먼저 팀과 상의한다.
// ============================================================================

// 함수 호출과 클래스 인스턴스 생성(Robot())에 동일하게 쓰인다 - 문법이 똑같이
// "표현식을 괄호로 호출"하는 것이기 때문이다. Executor가 callee를 평가한 값의
// 타입(Function/Class)에 따라 실제 동작을 구분한다.
struct CallExpression : public Expression {
    Expression* const callee;
    const std::vector<Expression*> arguments;

    CallExpression(const std::vector<Token>& tokens, Expression* callee, const std::vector<Expression*>& arguments)
        : Expression(tokens), callee(callee), arguments(arguments) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const CallExpression*>(&op);
        if (!node)
            return false;
        if (arguments.size() != node->arguments.size())
            return false;
        for (size_t i = 0; i < arguments.size(); i++) {
            if (!arguments[i]->operator==(*node->arguments[i]))
                return false;
        }
        return SyntaxNode::operator==(op) && callee->operator==(*node->callee);
    }
};

// r.name (필드 읽기), r.move(5)의 callee 자리(메서드 호출), alias.add(...)의
// callee 자리(import된 모듈 접근)에 모두 쓰인다. 대입 좌변(r.name = 3)으로 쓰일
// 때는 별도의 SetExpression 없이 AssignExpression::target이 이 노드를 그대로
// 가리킨다.
struct FieldAccessExpression : public Expression {
    Expression* const object;
    const Token name;

    FieldAccessExpression(const std::vector<Token>& tokens, Expression* object, const Token& name)
        : Expression(tokens), object(object), name(name) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const FieldAccessExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && object->operator==(*node->object) && name == node->name;
    }
};

// This. 클래스 메서드 실행 중에만 유효하며, 평가 방법은 IdentifierExpression과
// 동일하게 다뤄서(호출 시 스코프에 "this"라는 이름으로 바인딩) 정적 바인딩
// 최적화도 그대로 적용받을 수 있다 - Architecture.md §4.3 참고.
struct ThisExpression : public Expression {
    using Expression::Expression;

    bool operator==(const SyntaxNode& op) const override {
        return dynamic_cast<const ThisExpression*>(&op) != nullptr && SyntaxNode::operator==(op);
    }
};

// Array(3) 전용 문법. ARRAY가 예약어라 일반 CallExpression으로 파싱하지 않고
// 리터럴 파싱과 같은 층위에서 이 노드를 만든다.
struct ArrayExpression : public Expression {
    Expression* const sizeExpr;

    ArrayExpression(const std::vector<Token>& tokens, Expression* sizeExpr) : Expression(tokens), sizeExpr(sizeExpr) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const ArrayExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && sizeExpr->operator==(*node->sizeExpr);
    }
};

// arr[i] 읽기. 대입 좌변(arr[i] = 7)으로 쓰일 때는 FieldAccessExpression과
// 마찬가지로 별도 노드 없이 AssignExpression::target이 이 노드를 그대로 가리킨다.
struct IndexExpression : public Expression {
    Expression* const collection;
    Expression* const index;

    IndexExpression(const std::vector<Token>& tokens, Expression* collection, Expression* index)
        : Expression(tokens), collection(collection), index(index) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const IndexExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && collection->operator==(*node->collection) && index->operator==(*node->index);
    }
};

// a instanceof Robot. 우변은 항상 클래스 이름(식별자)이어야 하므로 별도
// Expression이 아니라 Token으로 받는다.
struct InstanceOfExpression : public Expression {
    Expression* const object;
    const Token className;

    InstanceOfExpression(const std::vector<Token>& tokens, Expression* object, const Token& className)
        : Expression(tokens), object(object), className(className) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const InstanceOfExpression*>(&op);
        if (!node)
            return false;
        return SyntaxNode::operator==(op) && object->operator==(*node->object) && className == node->className;
    }
};

// Func add(a, b) { ... }. 최상위 함수 선언 전용 노드다. params는 파라미터 이름
// 토큰 목록이다.
struct FunctionDeclareStatement : public Statement {
    const Token name;
    const std::vector<Token> params;
    const std::vector<Statement*> body;

    FunctionDeclareStatement(const std::vector<Token>& tokens, const Token& name, const std::vector<Token>& params,
        const std::vector<Statement*>& body)
        : Statement(tokens), name(name), params(params), body(body) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const FunctionDeclareStatement*>(&op);
        if (!node)
            return false;
        if (params != node->params)
            return false;
        if (body.size() != node->body.size())
            return false;
        for (size_t i = 0; i < body.size(); i++) {
            if (!body[i]->operator==(*node->body[i]))
                return false;
        }
        return SyntaxNode::operator==(op) && name == node->name;
    }
};

// move(dist) { ... }. 클래스 바디 안에서 FUNC 키워드 없이 선언되는 메서드
// 전용 노드다(3일차 슬라이드의 실제 문법) - Architecture.md §2.2/§4.1 참고.
// FunctionDeclareStatement와 필드 모양은 같지만, 문법이 서로 달라(하나는 FUNC로
// 시작, 하나는 바로 식별자로 시작) Assembler가 파싱 문맥을 노드 타입으로 표현할
// 수 있도록 별도 타입으로 둔다. 생성자도 별도 노드가 아니라 이름이 관례적으로
// "init"인 평범한 MethodDeclareStatement다.
struct MethodDeclareStatement : public Statement {
    const Token name;
    const std::vector<Token> params;
    const std::vector<Statement*> body;

    MethodDeclareStatement(const std::vector<Token>& tokens, const Token& name, const std::vector<Token>& params,
        const std::vector<Statement*>& body)
        : Statement(tokens), name(name), params(params), body(body) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const MethodDeclareStatement*>(&op);
        if (!node)
            return false;
        if (params != node->params)
            return false;
        if (body.size() != node->body.size())
            return false;
        for (size_t i = 0; i < body.size(); i++) {
            if (!body[i]->operator==(*node->body[i]))
                return false;
        }
        return SyntaxNode::operator==(op) && name == node->name;
    }
};

// return; 또는 return <expr>;. value는 없을 수 있다(nullptr = 빈 return).
struct ReturnStatement : public Statement {
    Expression* const value;

    ReturnStatement(const std::vector<Token>& tokens, Expression* value = nullptr) : Statement(tokens), value(value) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const ReturnStatement*>(&op);
        if (!node)
            return false;
        if ((value == nullptr) != (node->value == nullptr))
            return false;
        if (value && !value->operator==(*node->value))
            return false;
        return SyntaxNode::operator==(op);
    }
};

// Class Robot { ... }. 상속(Super, ":")은 3일차 1차 구현 범위에서 제외되어 있다 -
// 나중에 추가할 때는 superclass 필드만 덧붙이면 된다 (Architecture.md §4.5 참고).
struct ClassDeclareStatement : public Statement {
    const Token name;
    const std::vector<MethodDeclareStatement*> methods;

    ClassDeclareStatement(const std::vector<Token>& tokens, const Token& name, const std::vector<MethodDeclareStatement*>& methods)
        : Statement(tokens), name(name), methods(methods) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const ClassDeclareStatement*>(&op);
        if (!node)
            return false;
        if (methods.size() != node->methods.size())
            return false;
        for (size_t i = 0; i < methods.size(); i++) {
            if (!methods[i]->operator==(*node->methods[i]))
                return false;
        }
        return SyntaxNode::operator==(op) && name == node->name;
    }
};

// import "path" alias name;. declarations는 대상 파일에서 뽑아낸 최상위
// 선언들(VarDeclareStatement/FunctionDeclareStatement 등)이다. 파일을 읽고
// 파싱하는 일은 Assembler가 이 노드를 만드는 시점에 이미 끝나 있으므로
// Checker/Executor는 파일 시스템에 다시 접근할 필요가 없다 - Architecture.md §7
// 참고.
struct ImportStatement : public Statement {
    const Token alias;
    const std::vector<Statement*> declarations;

    ImportStatement(const std::vector<Token>& tokens, const Token& alias, const std::vector<Statement*>& declarations)
        : Statement(tokens), alias(alias), declarations(declarations) {}

    bool operator==(const SyntaxNode& op) const override {
        auto node = dynamic_cast<const ImportStatement*>(&op);
        if (!node)
            return false;
        if (declarations.size() != node->declarations.size())
            return false;
        for (size_t i = 0; i < declarations.size(); i++) {
            if (!declarations[i]->operator==(*node->declarations[i]))
                return false;
        }
        return SyntaxNode::operator==(op) && alias == node->alias;
    }
};
