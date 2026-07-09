#include "Executor.h"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "ArrayValue.h"
#include "InstanceValue.h"

namespace {

ExecutorError undefinedVariableError(const std::string& name) {
    return ExecutorError("'{}' 변수가 정의되지 않았습니다.", name);
}

// return 문이 함수/메서드 호출 스택을 즉시 빠져나가기 위해 던지는 내부 전용
// 제어 흐름 신호. Executor 밖으로 노출하지 않는다(공개 헤더에 없음) - 일반
// ExecutorError와 섞이지 않도록 별도 타입으로 둔다.
struct ReturnSignal {
    Value value;
};

// import 대상 파일의 최상위 선언 하나가 등록하는 이름을 알아낸다. moduleScope로
// 옮길 값을 찾을 때 쓴다 - Scope에 "내용을 훑는" API를 새로 추가하지 않고도,
// 어떤 이름이 선언될지는 노드 자체에서 이미 알 수 있기 때문이다.
std::string declaredNameOf(Statement* decl) {
    if (auto* var = dynamic_cast<DeclareStatement*>(decl)) {
        return var->identifier->name;
    }
    if (auto* func = dynamic_cast<FunctionDeclareStatement*>(decl)) {
        return func->name.origin;
    }
    if (auto* klass = dynamic_cast<ClassDeclareStatement*>(decl)) {
        return klass->name.origin;
    }
    throw ExecutorError("import 대상으로 지원하지 않는 선언입니다.");
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

Executor::Executor(std::ostream& out) : out_(out) {}

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

void Executor::requireNumberOperands(const Value& left, const Value& right, const char* op) const {
    if (!left.isNumber() || !right.isNumber())
        throw ExecutorError("타입 오류: {} {} {}", left.typeName(), op, right.typeName());
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
    throw ExecutorError("타입 오류: {} + {}", left.typeName(), right.typeName());
}

void Executor::visit(SubExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, "-");
    lastValue_ = Value(left.asNumber() - right.asNumber());
}

void Executor::visit(MultExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, "*");
    lastValue_ = Value(left.asNumber() * right.asNumber());
}

void Executor::visit(DivideExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, "/");
    if (right.asNumber() == 0.0)
        throw ExecutorError("0으로 나눌 수 없습니다");
    lastValue_ = Value(left.asNumber() / right.asNumber());
}

void Executor::visit(NegativeExpression& node) {
    Value operand = evaluate(node.operand);
    if (!operand.isNumber())
        throw ExecutorError("타입 오류: -{}", operand.typeName());
    lastValue_ = Value(-operand.asNumber());
}

void Executor::visit(LessExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, "<");
    lastValue_ = Value(left.asNumber() < right.asNumber());
}

void Executor::visit(GreaterExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, ">");
    lastValue_ = Value(left.asNumber() > right.asNumber());
}

void Executor::visit(IdentifierExpression& node) {
    // depth가 채워져 있으면(Checker의 Resolver가 계산한 정적 바인딩 결과,
    // Architecture.md §6.1) 스코프를 훑지 않고 바로 그 스코프에서 조회한다.
    // 아직 아무도 depth를 채우지 않는 동안에는(Resolver 미구현) 항상
    // nullopt라서 기존과 동일하게 동적 조회로 동작한다.
    auto value = node.depth
        ? environment_.lookupAt(*node.depth, node.name)
        : environment_.lookup(node.name);
    if (!value) {
        throw undefinedVariableError(node.name);
    }
    lastValue_ = *value;
}

void Executor::visit(AssignExpression& node) {
    if (auto* fieldTarget = dynamic_cast<FieldAccessExpression*>(node.target)) {
        Value object = evaluate(fieldTarget->object);
        if (!object.isInstance()) {
            throw ExecutorError("인스턴스가 아닌 대상에 필드를 대입했습니다.");
        }
        Value value = evaluate(node.value);
        object.asInstance()->fields->define(fieldTarget->name.origin, value);  // 없으면 새로 생성.
        lastValue_ = value;
        return;
    }

    if (auto* indexTarget = dynamic_cast<IndexExpression*>(node.target)) {
        auto [array, i] = resolveArrayIndex(indexTarget->collection, indexTarget->index);
        Value value = evaluate(node.value);
        array->items[i] = value;
        lastValue_ = value;
        return;
    }

    auto* identifier = dynamic_cast<IdentifierExpression*>(node.target);
    if (!identifier) {
        throw ExecutorError("아직 지원하지 않는 대입 대상입니다.");
    }
    Value value = evaluate(node.value);
    bool assigned = identifier->depth
        ? environment_.assignAt(*identifier->depth, identifier->name, value)
        : environment_.assign(identifier->name, value);
    if (!assigned) {
        throw undefinedVariableError(identifier->name);
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
    requireNumberOperands(left, right, "<=");
    lastValue_ = Value(left.asNumber() <= right.asNumber());
}

void Executor::visit(GreaterEqualExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, ">=");
    lastValue_ = Value(left.asNumber() >= right.asNumber());
}

void Executor::visit(NotExpression& node) {
    lastValue_ = Value(!evaluate(node.operand).isTruthy());
}

void Executor::visit(ModExpression& node) {
    Value left = evaluate(node.left);
    Value right = evaluate(node.right);
    requireNumberOperands(left, right, "%");
    if (right.asNumber() == 0.0)
        throw ExecutorError("0으로 나눌 수 없습니다");
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
        "Executor::visit(MethodDeclareStatement&): 메서드는 execute()로 직접 방문되지 않는다 - "
        "invoke()를 통해서만 호출된다.");
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
        lastValue_ = instantiate(callee.asClass(), args);
        return;
    }
    throw ExecutorError("호출할 수 없는 대상입니다.");
}

void Executor::visit(ClassDeclareStatement& node) {
    environment_.define(node.name.origin, Value(&node));
}

void Executor::visit(ImportStatement& node) {
    auto moduleScope = std::make_shared<Scope>();

    {
        ScopeGuard guard(environment_);  // declarations 실행용 임시 프레임.
        for (Statement* decl : node.declarations) {
            execute(decl);
            std::string name = declaredNameOf(decl);
            moduleScope->define(name, *environment_.lookup(name));
        }
    }  // 임시 프레임은 여기서 pop된다 - alias는 바깥(호출 시점) 스코프에 등록한다.

    environment_.define(node.alias.origin, Value(moduleScope));
}

void Executor::visit(FieldAccessExpression& node) {
    Value object = evaluate(node.object);
    if (object.isModule()) {
        if (auto value = object.asModule()->get(node.name.origin)) {
            lastValue_ = *value;
            return;
        }
        throw ExecutorError("모듈에 '{}'이(가) 없습니다.", node.name.origin);
    }
    if (!object.isInstance()) {
        throw ExecutorError("인스턴스가 아닌 대상의 필드에 접근했습니다.");
    }
    if (auto field = object.asInstance()->fields->get(node.name.origin)) {
        lastValue_ = *field;
        return;
    }
    // 값 읽기 문맥에서 메서드에 접근한 경우: 메서드 호출은 CallExpression
    // 쪽(callMethod)이 전담하므로 여기서는 에러로 처리한다.
    throw ExecutorError("'{}' 필드가 존재하지 않습니다.", node.name.origin);
}

void Executor::visit(ArrayExpression& node) {
    Value size = evaluate(node.sizeExpr);
    if (!size.isNumber()) {
        throw ExecutorError("배열의 사이즈는 반드시 number여야 합니다.");
    }
    auto array = std::make_shared<ArrayValue>();
    array->items.resize(static_cast<size_t>(size.asNumber()));  // 전부 Nil로 채워짐.
    lastValue_ = Value(array);
}

void Executor::visit(IndexExpression& node) {
    auto [array, i] = resolveArrayIndex(node.collection, node.index);
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
        throw ExecutorError("'{}'은(는) 클래스가 아닙니다.", node.className.origin);
    }
    // 자기 자신뿐 아니라 superclass 체인 어딘가와 일치해도 true.
    for (const ClassDeclareStatement* k = object.asInstance()->klass; k != nullptr; k = resolveSuperclass(k)) {
        if (k == classValue->asClass()) {
            lastValue_ = Value(true);
            return;
        }
    }
    lastValue_ = Value(false);
}

void Executor::visit(ThisExpression&) {
    // This는 항상 "this"라는 고정 이름으로 동적 조회한다 - 메서드 호출
    // 스코프 최상단에 있어서 조회 비용이 낮고, depth 캐싱의 이점이 작다.
    auto value = environment_.lookup("this");
    if (!value) {
        throw ExecutorError("클래스 외부에서 This를 사용했습니다.");
    }
    lastValue_ = *value;
}

void Executor::visit(SuperExpression&) {
    // Super.field는 This.field와 완전히 동일하게 동작한다 - 필드 저장소가
    // 클래스 계층과 무관하게 인스턴스당 하나(instance->fields)이기 때문이다.
    // Super.method(...) 호출은 이 방문을 타지 않고 callMethod가 먼저
    // 가로챈다(메서드 탐색 시작점을 superclass로 옮겨야 하므로).
    auto value = environment_.lookup("this");
    if (!value) {
        throw ExecutorError("클래스 외부에서 Super를 사용했습니다.");
    }
    lastValue_ = *value;
}

Value Executor::invoke(const Token& name, const std::vector<Token>& params, const std::vector<Statement*>& body,
    const std::vector<Value>& args, std::optional<Value> boundThis) {
    if (args.size() != params.size()) {
        throw ExecutorError("'{}' 호출에는 인자 {}개가 필요합니다 (전달된 인자: {}개)",
            name.origin, params.size(), args.size());
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

Value Executor::instantiate(const ClassDeclareStatement* klass, const std::vector<Value>& args) {
    auto instance = std::make_shared<InstanceValue>();
    instance->klass = klass;
    instance->fields = std::make_shared<Scope>();
    Value instanceValue(instance);

    if (MethodDeclareStatement* init = findMethod(klass, "init")) {
        callMethodDecl(init, args, instanceValue);  // 반환값은 버린다.
    }
    return instanceValue;
}

MethodDeclareStatement* Executor::findMethod(const ClassDeclareStatement* klass, const std::string& name) {
    for (const ClassDeclareStatement* k = klass; k != nullptr; k = resolveSuperclass(k)) {
        for (MethodDeclareStatement* method : k->methods) {
            if (method->name.origin == name) {
                return method;
            }
        }
    }
    return nullptr;
}

const ClassDeclareStatement* Executor::resolveSuperclass(const ClassDeclareStatement* klass) {
    if (klass->superclass == nullptr) {
        return nullptr;
    }
    const IdentifierExpression* superclass = klass->superclass;
    auto value = superclass->depth
        ? environment_.lookupAt(*superclass->depth, superclass->name)
        : environment_.lookup(superclass->name);
    if (!value || !value->isClass()) {
        throw ExecutorError("'{}'은(는) 클래스가 아닙니다.", superclass->name);
    }
    return value->asClass();
}

Value Executor::callMethod(FieldAccessExpression* fieldAccess, const std::vector<Expression*>& argExprs) {
    if (dynamic_cast<SuperExpression*>(fieldAccess->object)) {
        // Super.method(...): this는 그대로 유지한 채, 메서드 탐색 시작점만
        // this가 속한 클래스가 아니라 superclass로 강제 이동한다.
        auto thisValue = environment_.lookup("this");
        if (!thisValue || !thisValue->isInstance()) {
            throw ExecutorError("클래스 외부에서 Super를 사용했습니다.");
        }
        const ClassDeclareStatement* startClass = resolveSuperclass(thisValue->asInstance()->klass);
        MethodDeclareStatement* method = startClass ? findMethod(startClass, fieldAccess->name.origin) : nullptr;
        if (!method) {
            throw ExecutorError("'{}' 메서드가 부모 클래스에 존재하지 않습니다.", fieldAccess->name.origin);
        }
        std::vector<Value> args;
        args.reserve(argExprs.size());
        for (Expression* arg : argExprs) {
            args.push_back(evaluate(arg));
        }
        return callMethodDecl(method, args, *thisValue);
    }

    Value object = evaluate(fieldAccess->object);

    if (object.isModule()) {
        // alias.add(...): 모듈 스코프에서 이름을 찾아 함수처럼 호출한다.
        auto member = object.asModule()->get(fieldAccess->name.origin);
        if (!member || !member->isFunction()) {
            throw ExecutorError("모듈에 '{}' 함수가 없습니다.", fieldAccess->name.origin);
        }
        std::vector<Value> moduleArgs;
        moduleArgs.reserve(argExprs.size());
        for (Expression* arg : argExprs) {
            moduleArgs.push_back(evaluate(arg));
        }
        return callFunction(member->asFunction(), moduleArgs);
    }

    if (!object.isInstance()) {
        throw ExecutorError("인스턴스가 아닌 대상의 메서드를 호출했습니다.");
    }
    auto& instance = object.asInstance();
    MethodDeclareStatement* method = findMethod(instance->klass, fieldAccess->name.origin);
    if (!method) {
        throw ExecutorError("'{}' 메서드가 존재하지 않습니다.", fieldAccess->name.origin);
    }
    std::vector<Value> args;
    args.reserve(argExprs.size());
    for (Expression* arg : argExprs) {
        args.push_back(evaluate(arg));
    }
    return callMethodDecl(method, args, object);
}

std::pair<std::shared_ptr<ArrayValue>, size_t> Executor::resolveArrayIndex(Expression* collectionExpr, Expression* indexExpr) {
    Value collection = evaluate(collectionExpr);
    if (!collection.isArray()) {
        throw ExecutorError("index 접근은 오직 배열만 지원합니다.");
    }
    Value indexValue = evaluate(indexExpr);
    if (!indexValue.isNumber()) {
        throw ExecutorError("인덱스는 반드시 숫자여야 합니다.");
    }
    auto array = collection.asArray();
    auto i = static_cast<size_t>(indexValue.asNumber());
    if (i >= array->items.size()) {
        throw ExecutorError("배열 인덱스 범위를 벗어났습니다.");
    }
    return { array, i };
}
