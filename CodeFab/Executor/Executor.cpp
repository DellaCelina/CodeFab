#include "Executor.h"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "ArrayValue.h"
#include "InstanceValue.h"

namespace {

ExecutorError undefinedVariableError(const std::string& name, int line) {
    return ExecutorError("[line {}] '{}' is not defined.", line, name);
}

// Environment::lookupAt/assignAt은 depth가 스코프 범위를 벗어나면
// std::out_of_range를 던진다 - 사용자 코드 문제가 아니라 Resolver가 계산한
// depth가 잘못됐다는, 우리 프로그램 자체의 버그 신호다. 그래도 Executor 밖으로는
// ExecutorError만 노출한다는 규칙(README)을 지키기 위해 여기서 다시 감싼다.
template <typename Func>
auto guardScopeAccess(Func&& func) {
    try {
        return func();
    } catch (const std::out_of_range& e) {
        throw ExecutorError("internal error: {}", e.what());
    }
}

// return 문이 함수/메서드 호출 스택을 즉시 빠져나가기 위해 던지는 내부 전용
// 제어 흐름 신호. Executor 밖으로 노출하지 않는다(공개 헤더에 없음) - 일반
// ExecutorError와 섞이지 않도록 별도 타입으로 둔다.
struct ReturnSignal {
    Value value;
};
}  // namespace

Executor::Executor(std::ostream& out)
    : out_(out), classRuntime_(*this), moduleRuntime_(*this), arrayRuntime_(*this) {}

void Executor::execute(SyntaxTree& tree) {
    auto* root = dynamic_cast<Statement*>(tree.getRoot());
    if (!root) {
        throw std::logic_error("Executor::execute: tree root is not a Statement");
    }
    execute(root);
}

void Executor::execute(Statement* stmt) {
    // 재귀 호출(Block/If/For 바디, 함수/메서드 호출 등)에도 depth가 항상
    // 정확히 감소하도록 RAII로 처리한다 - 핸들러가 ReturnSignal/ExecutorError를
    // 던지고 그 예외가 이 프레임을 그냥 지나가도 statementDepth_는 어긋나지 않는다.
    ++statementDepth_;
    struct DepthGuard {
        int& depth;
        ~DepthGuard() { --depth; }
    } depthGuard{statementDepth_};

    if (statementHook_) {
        statementHook_(stmt, statementDepth_);
    }
    stmt->accept(*this);
}

Value Executor::evaluate(Expression* expr) {
    expr->accept(*this);
    return std::move(lastValue_);
}

const Environment& Executor::environment() const {
    return environment_;
}

void Executor::setStatementHook(StatementHook hook) {
    statementHook_ = std::move(hook);
}

void Executor::requireNumberOperands(const Value& left, const Value& right, const char* op, int line) const {
    if (!left.isNumber() || !right.isNumber())
        throw ExecutorError("[line {}] type error: {} {} {}", line, left.typeName(), op, right.typeName());
}

void Executor::visit(PrintStatement& stmt) {
    out_ << evaluate(stmt.expr).toString() << std::endl;
}

void Executor::visit(NumberExpression& node) {
    lastValue_ = Value(node.value);
}

void Executor::visit(StringExpression& node) {
    lastValue_ = Value(node.value);
}

void Executor::visit(BooleanExpression& node) {
    lastValue_ = Value(node.value);
}

void Executor::visit(AddExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    if (left.isString() && right.isString()) {
        lastValue_ = Value(left.asString() + right.asString());
        return;
    }
    if (left.isNumber() && right.isNumber()) {
        lastValue_ = Value(left.asNumber() + right.asNumber());
        return;
    }
    throw ExecutorError("[line {}] type error: {} + {}", node.getLine(), left.typeName(), right.typeName());
}

void Executor::visit(SubExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, "-", node.getLine());
    lastValue_ = Value(left.asNumber() - right.asNumber());
}

void Executor::visit(MultExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, "*", node.getLine());
    lastValue_ = Value(left.asNumber() * right.asNumber());
}

void Executor::visit(DivideExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, "/", node.getLine());
    if (right.asNumber() == 0.0)
        throw ExecutorError("[line {}] division by zero.", node.getLine());
    lastValue_ = Value(left.asNumber() / right.asNumber());
}

void Executor::visit(NegativeExpression& node) {
    Value operand = evaluate(node.operand);
    if (!operand.isNumber())
        throw ExecutorError("[line {}] type error: unary '-' requires a number, got {}.", node.getLine(), operand.typeName());
    lastValue_ = Value(-operand.asNumber());
}

void Executor::visit(LessExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, "<", node.getLine());
    lastValue_ = Value(left.asNumber() < right.asNumber());
}

void Executor::visit(GreaterExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, ">", node.getLine());
    lastValue_ = Value(left.asNumber() > right.asNumber());
}

void Executor::visit(IdentifierExpression& node) {
    // depth가 채워져 있으면(Checker의 Resolver가 계산한 정적 바인딩 결과,
    // Architecture.md §6.1) 스코프를 훑지 않고 바로 그 스코프에서 조회한다.
    // 아직 아무도 depth를 채우지 않는 동안에는(Resolver 미구현) 항상
    // nullopt라서 기존과 동일하게 동적 조회로 동작한다.
    auto value = node.depth
        ? guardScopeAccess([&] { return environment_.lookupAt(*node.depth, node.name); })
        : environment_.lookup(node.name);
    if (!value) {
        throw undefinedVariableError(node.name, node.getLine());
    }
    lastValue_ = *value;
}

void Executor::visit(AssignExpression& node) {
    if (auto* fieldTarget = dynamic_cast<FieldAccessExpression*>(node.target)) {
        Value object = evaluate(fieldTarget->object);
        if (!object.isInstance()) {
            throw ExecutorError("[line {}] cannot assign field to a non-instance.", node.getLine());
        }
        Value value = evaluate(node.value);
        object.asInstance()->fields->define(fieldTarget->name.origin, value);  // 없으면 새로 생성.
        lastValue_ = value;
        return;
    }

    if (auto* indexTarget = dynamic_cast<IndexExpression*>(node.target)) {
        auto [array, i] = arrayRuntime_.resolveIndex(indexTarget->collection, indexTarget->index);
        Value value = evaluate(node.value);
        array->items[i] = value;
        lastValue_ = value;
        return;
    }

    auto* identifier = dynamic_cast<IdentifierExpression*>(node.target);
    if (!identifier) {
        throw ExecutorError("[line {}] invalid assignment target.", node.getLine());
    }
    Value value = evaluate(node.value);
    bool assigned = identifier->depth
        ? guardScopeAccess([&] { return environment_.assignAt(*identifier->depth, identifier->name, value); })
        : environment_.assign(identifier->name, value);
    if (!assigned) {
        throw undefinedVariableError(identifier->name, node.getLine());
    }
    lastValue_ = value;
}

void Executor::visit(DeclareStatement& node) {
    environment_.define(node.identifier->name, evaluate(node.expr));
}

void Executor::visit(ExpressionStatement& node) {
    evaluate(node.expr);
}

void Executor::visit(BlockStatement& node) {
    ScopeGuard guard(environment_);
    for (Statement* inner : node.statements) {
        execute(inner);
    }
}

void Executor::visit(IfStatement& node) {
    if (evaluate(node.expr).isTruthy()) {
        execute(node.thenBranch);
    } else if (node.elseBranch) {
        execute(node.elseBranch);
    }
}

void Executor::visit(ForStatement& node) {
    // 초기화절에서 선언한 변수(예: var j = 0)가 for문이 끝난 뒤 바깥으로
    // 새어나가지 않도록 전용 스코프를 둔다.
    ScopeGuard guard(environment_);
    execute(node.init);
    while (evaluate(node.compare).isTruthy()) {
        execute(node.loop);
        evaluate(node.next);
    }
}

void Executor::visit(EqualExpression& node) {
    lastValue_ = Value(evaluate(node.left) == evaluate(node.right));
}

void Executor::visit(NotEqualExpression& node) {
    lastValue_ = Value(!(evaluate(node.left) == evaluate(node.right)));
}

void Executor::visit(LessEqualExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, "<=", node.getLine());
    lastValue_ = Value(left.asNumber() <= right.asNumber());
}

void Executor::visit(GreaterEqualExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, ">=", node.getLine());
    lastValue_ = Value(left.asNumber() >= right.asNumber());
}

void Executor::visit(NotExpression& node) {
    lastValue_ = Value(!evaluate(node.operand).isTruthy());
}

void Executor::visit(ModExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, "%", node.getLine());
    if (right.asNumber() == 0.0)
        throw ExecutorError("[line {}] division by zero.", node.getLine());
    lastValue_ = Value(std::fmod(left.asNumber(), right.asNumber()));
}

void Executor::visit(AndExpression& node) {
    Value left = evaluate(node.left);
    if (!left.isTruthy()) {
        lastValue_ = Value(false);
        return;
    }
    lastValue_ = Value(evaluate(node.right).isTruthy());
}

void Executor::visit(OrExpression& node) {
    Value left = evaluate(node.left);
    if (left.isTruthy()) {
        lastValue_ = Value(true);
        return;
    }
    lastValue_ = Value(evaluate(node.right).isTruthy());
}

void Executor::visit(FunctionDeclareStatement& node) {
    // 선언 시점에 이름부터 현재 스코프에 등록해둔다 - 그래야 body 안에서
    // 자기 자신을 부르는 재귀 호출이 자연스럽게 동작한다(호출은 실제로
    // 실행될 때 일어나므로, 그 시점엔 이미 이름이 등록되어 있다).
    environment_.define(node.name.origin, Value(&node));
}

void Executor::visit(MethodDeclareStatement&) {
    throw std::logic_error(
        "Executor::visit(MethodDeclareStatement&): methods are not visited directly via execute() - "
        "use invoke() instead.");
}

void Executor::visit(ReturnStatement& node) {
    throw ReturnSignal{ node.value ? evaluate(node.value) : Value() };
}

void Executor::visit(CallExpression& node) {
    // callee가 r.move(5)처럼 FieldAccessExpression이면 메서드 호출이다 -
    // object를 "값으로 읽는" 일반 FieldAccessExpression 평가(필드 조회)를
    // 타지 않도록 여기서 먼저 가로챈다.
    if (auto* fieldAccess = dynamic_cast<FieldAccessExpression*>(node.callee)) {
        lastValue_ = callMethod(fieldAccess, node.arguments);
        return;
    }

    Value callee = evaluate(node.callee);
    std::vector<Value> args;
    args.reserve(node.arguments.size());
    for (Expression* arg : node.arguments) {
        args.push_back(evaluate(arg));
    }

    if (callee.isFunction()) {
        lastValue_ = callFunction(callee.asFunction(), args);
        return;
    }
    if (callee.isClass()) {
        lastValue_ = classRuntime_.instantiate(callee.asClass(), args);
        return;
    }
    throw ExecutorError("[line {}] callee is not callable.", node.getLine());
}

void Executor::visit(ClassDeclareStatement& node) {
    environment_.define(node.name.origin, Value(&node));
}

void Executor::visit(ImportStatement& node) {
    moduleRuntime_.runImport(node);
}

void Executor::visit(FieldAccessExpression& node) {
    Value object = evaluate(node.object);
    if (object.isModule()) {
        if (auto value = object.asModule()->get(node.name.origin)) {
            lastValue_ = *value;
            return;
        }
        throw ExecutorError("[line {}] '{}' is not defined in module.", node.getLine(), node.name.origin);
    }
    if (!object.isInstance()) {
        throw ExecutorError("[line {}] cannot access field on a non-instance.", node.getLine());
    }
    if (auto field = object.asInstance()->fields->get(node.name.origin)) {
        lastValue_ = *field;
        return;
    }
    // 값 읽기 문맥에서 메서드에 접근한 경우: 메서드 호출은 CallExpression
    // 쪽(callMethod)이 전담하므로 여기서는 에러로 처리한다.
    throw ExecutorError("[line {}] field '{}' does not exist.", node.getLine(), node.name.origin);
}

void Executor::visit(ArrayExpression& node) {
    lastValue_ = arrayRuntime_.create(node.sizeExpr);
}

void Executor::visit(IndexExpression& node) {
    auto [array, i] = arrayRuntime_.resolveIndex(node.collection, node.index);
    lastValue_ = array->items[i];
}

void Executor::visit(InstanceOfExpression& node) {
    Value object = evaluate(node.object);
    if (!object.isInstance()) {
        lastValue_ = Value(false);
        return;
    }
    auto classValue = environment_.lookup(node.className.origin);
    if (!classValue || !classValue->isClass()) {
        throw ExecutorError("[line {}] '{}' is not a class.", node.getLine(), node.className.origin);
    }
    lastValue_ = Value(classRuntime_.isInstanceOf(object.asInstance()->klass, classValue->asClass()));
}

void Executor::visit(ThisExpression& node) {
    // This는 항상 "this"라는 고정 이름으로 동적 조회한다 - 메서드 호출
    // 스코프 최상단에 있어서 조회 비용이 낮고, depth 캐싱의 이점이 작다.
    auto value = environment_.lookup("this");
    if (!value) {
        throw ExecutorError("[line {}] cannot use 'This' outside a class method.", node.getLine());
    }
    lastValue_ = *value;
}

void Executor::visit(SuperExpression& node) {
    // Super.field는 This.field와 완전히 동일하게 동작한다 - 필드 저장소가
    // 클래스 계층과 무관하게 인스턴스당 하나(instance->fields)이기 때문이다.
    // Super.method(...) 호출은 이 방문을 타지 않고 callMethod가 먼저
    // 가로챈다(메서드 탐색 시작점을 superclass로 옮겨야 하므로).
    auto value = environment_.lookup("this");
    if (!value) {
        throw ExecutorError("[line {}] cannot use 'Super' outside a class method.", node.getLine());
    }
    lastValue_ = *value;
}

Value Executor::invoke(const Token& name, const std::vector<Token>& params, const std::vector<Statement*>& body,
    const std::vector<Value>& args, std::optional<Value> boundThis) {
    if (args.size() != params.size()) {
        throw ExecutorError("[line {}] '{}' expects {} argument(s) but got {}.",
            name.line, name.origin, params.size(), args.size());
    }

    // ScopeGuard가 소멸자에서 popScope()를 보장하므로, body 실행 중 던져진
    // ReturnSignal이나 다른 예외로 스택을 빠져나가도 스코프가 안전하게 정리된다.
    ScopeGuard guard(environment_);
    if (boundThis) {
        environment_.define("this", *boundThis);
    }
    for (size_t i = 0; i < params.size(); ++i) {
        environment_.define(params[i].origin, args[i]);
    }

    try {
        for (Statement* stmt : body) {
            execute(stmt);
        }
    } catch (const ReturnSignal& ret) {
        return ret.value;
    }
    return Value();  // return 없이 끝나면 Nil.
}

Value Executor::callFunction(const FunctionDeclareStatement* decl, const std::vector<Value>& args) {
    return invoke(decl->name, decl->params, decl->body, args);
}

Value Executor::callMethodDecl(const MethodDeclareStatement* method, const std::vector<Value>& args, Value boundThis) {
    return invoke(method->name, method->params, method->body, args, boundThis);
}

Value Executor::callMethod(FieldAccessExpression* fieldAccess, const std::vector<Expression*>& argExprs) {
    // Super.method(...)는 object를 "값으로" 평가하지 않는다 - 탐색 시작점을
    // superclass로 옮기는 것 자체가 목적이라 ClassRuntime이 직접 처리한다.
    if (dynamic_cast<SuperExpression*>(fieldAccess->object)) {
        return classRuntime_.callSuperMethod(fieldAccess, argExprs);
    }

    Value object = evaluate(fieldAccess->object);
    if (object.isModule()) {
        return moduleRuntime_.callMember(object, fieldAccess, argExprs);
    }
    return classRuntime_.callInstanceMethod(object, fieldAccess, argExprs);
}
