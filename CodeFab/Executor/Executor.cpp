#include "Executor.h"

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
        // depth가 채워져 있으면(Checker의 Resolver가 계산한 정적 바인딩 결과,
        // Architecture.md §6.1) 스코프를 훑지 않고 바로 그 스코프에서 조회한다.
        // 아직 아무도 depth를 채우지 않는 동안에는(Resolver 미구현) 항상
        // nullopt라서 기존과 동일하게 동적 조회로 동작한다.
        auto value = identifier->depth
            ? environment_.lookupAt(*identifier->depth, identifier->name)
            : environment_.lookup(identifier->name);
        if (!value) {
            throw undefinedVariableError(identifier->name);
        }
        return *value;
    };

    expressionHandlers_[std::type_index(typeid(AssignExpression))] = [this](Expression* expr) {
        auto* assign = static_cast<AssignExpression*>(expr);

        if (auto* fieldTarget = dynamic_cast<FieldAccessExpression*>(assign->target)) {
            Value object = evaluate(fieldTarget->object);
            if (!object.isInstance()) {
                throw ExecutorError("인스턴스가 아닌 대상에 필드를 대입했습니다.");
            }
            Value value = evaluate(assign->value);
            object.asInstance()->fields->define(fieldTarget->name.origin, value);  // 없으면 새로 생성.
            return value;
        }

        if (auto* indexTarget = dynamic_cast<IndexExpression*>(assign->target)) {
            auto [array, i] = resolveArrayIndex(indexTarget->collection, indexTarget->index);
            Value value = evaluate(assign->value);
            array->items[i] = value;
            return value;
        }

        auto* identifier = dynamic_cast<IdentifierExpression*>(assign->target);
        if (!identifier) {
            throw ExecutorError("아직 지원하지 않는 대입 대상입니다.");
        }
        Value value = evaluate(assign->value);
        bool assigned = identifier->depth
            ? environment_.assignAt(*identifier->depth, identifier->name, value)
            : environment_.assign(identifier->name, value);
        if (!assigned) {
            throw undefinedVariableError(identifier->name);
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
        // 초기화절에서 선언한 변수(예: var j = 0)가 for문이 끝난 뒤 바깥으로
        // 새어나가지 않도록 전용 스코프를 둔다.
        ScopeGuard guard(environment_);
        execute(forStmt->init);
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

    statementHandlers_[std::type_index(typeid(FunctionDeclareStatement))] = [this](Statement* stmt) {
        auto* decl = static_cast<FunctionDeclareStatement*>(stmt);
        // 선언 시점에 이름부터 현재 스코프에 등록해둔다 - 그래야 body 안에서
        // 자기 자신을 부르는 재귀 호출이 자연스럽게 동작한다(호출은 실제로
        // 실행될 때 일어나므로, 그 시점엔 이미 이름이 등록되어 있다).
        environment_.define(decl->name.origin, Value(decl));
    };

    statementHandlers_[std::type_index(typeid(ReturnStatement))] = [this](Statement* stmt) {
        auto* ret = static_cast<ReturnStatement*>(stmt);
        throw ReturnSignal{ ret->value ? evaluate(ret->value) : Value() };
    };

    expressionHandlers_[std::type_index(typeid(CallExpression))] = [this](Expression* expr) {
        auto* call = static_cast<CallExpression*>(expr);

        // callee가 r.move(5)처럼 FieldAccessExpression이면 메서드 호출이다 -
        // object를 "값으로 읽는" 일반 FieldAccessExpression 평가(필드 조회)를
        // 타지 않도록 여기서 먼저 가로챈다.
        if (auto* fieldAccess = dynamic_cast<FieldAccessExpression*>(call->callee)) {
            return callMethod(fieldAccess, call->arguments);
        }

        Value callee = evaluate(call->callee);
        std::vector<Value> args;
        args.reserve(call->arguments.size());
        for (Expression* arg : call->arguments) {
            args.push_back(evaluate(arg));
        }

        if (callee.isFunction()) {
            return callFunction(callee.asFunction(), args);
        }
        if (callee.isClass()) {
            return instantiate(callee.asClass(), args);
        }
        throw ExecutorError("호출할 수 없는 대상입니다.");
    };

    statementHandlers_[std::type_index(typeid(ClassDeclareStatement))] = [this](Statement* stmt) {
        auto* decl = static_cast<ClassDeclareStatement*>(stmt);
        environment_.define(decl->name.origin, Value(decl));
    };

    expressionHandlers_[std::type_index(typeid(FieldAccessExpression))] = [this](Expression* expr) {
        auto* access = static_cast<FieldAccessExpression*>(expr);
        Value object = evaluate(access->object);
        if (!object.isInstance()) {
            throw ExecutorError("인스턴스가 아닌 대상의 필드에 접근했습니다.");
        }
        if (auto field = object.asInstance()->fields->get(access->name.origin)) {
            return *field;
        }
        // 값 읽기 문맥에서 메서드에 접근한 경우: 메서드 호출은 CallExpression
        // 쪽(callMethod)이 전담하므로 여기서는 에러로 처리한다.
        throw ExecutorError("'{}' 필드가 존재하지 않습니다.", access->name.origin);
    };

    expressionHandlers_[std::type_index(typeid(ArrayExpression))] = [this](Expression* expr) {
        auto* arrayExpr = static_cast<ArrayExpression*>(expr);
        Value size = evaluate(arrayExpr->sizeExpr);
        if (!size.isNumber()) {
            throw ExecutorError("배열의 사이즈는 반드시 number여야 합니다.");
        }
        auto array = std::make_shared<ArrayValue>();
        array->items.resize(static_cast<size_t>(size.asNumber()));  // 전부 Nil로 채워짐.
        return Value(array);
    };

    expressionHandlers_[std::type_index(typeid(IndexExpression))] = [this](Expression* expr) {
        auto* index = static_cast<IndexExpression*>(expr);
        auto [array, i] = resolveArrayIndex(index->collection, index->index);
        return array->items[i];
    };

    expressionHandlers_[std::type_index(typeid(ThisExpression))] = [this](Expression*) {
        // This는 항상 "this"라는 고정 이름으로 동적 조회한다 - 메서드 호출
        // 스코프 최상단에 있어서 조회 비용이 낮고, depth 캐싱의 이점이 작다.
        auto value = environment_.lookup("this");
        if (!value) {
            throw ExecutorError("클래스 외부에서 This를 사용했습니다.");
        }
        return *value;
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
    if (statementHook_) {
        statementHook_(stmt);
    }
    auto it = statementHandlers_.find(std::type_index(typeid(*stmt)));
    if (it == statementHandlers_.end()) {
        throw std::logic_error("Executor::execute: no handler registered for this statement node");
    }
    it->second(stmt);
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

    for (MethodDeclareStatement* method : klass->methods) {
        if (method->name.origin == "init") {
            callMethodDecl(method, args, instanceValue);  // 반환값은 버린다.
            break;
        }
    }
    return instanceValue;
}

Value Executor::callMethod(FieldAccessExpression* fieldAccess, const std::vector<Expression*>& argExprs) {
    Value object = evaluate(fieldAccess->object);
    if (!object.isInstance()) {
        throw ExecutorError("인스턴스가 아닌 대상의 메서드를 호출했습니다.");
    }
    auto& instance = object.asInstance();
    MethodDeclareStatement* method = nullptr;
    for (MethodDeclareStatement* candidate : instance->klass->methods) {
        if (candidate->name.origin == fieldAccess->name.origin) {
            method = candidate;
            break;
        }
    }
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

Value Executor::evaluate(Expression* expr) {
    auto it = expressionHandlers_.find(std::type_index(typeid(*expr)));
    if (it == expressionHandlers_.end()) {
        throw std::logic_error("Executor::evaluate: no handler registered for this expression node");
    }
    return it->second(expr);
}
