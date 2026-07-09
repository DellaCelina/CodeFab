#include "ClassRuntime.h"

#include "Executor.h"
#include "InstanceValue.h"

ClassRuntime::ClassRuntime(Executor& executor) : executor_(executor) {}

Value ClassRuntime::instantiate(const ClassDeclareStatement* klass, const std::vector<Value>& args) {
    auto instance = std::make_shared<InstanceValue>();
    instance->klass = klass;
    instance->fields = std::make_shared<Scope>();
    Value instanceValue(instance);

    if (MethodDeclareStatement* init = findMethod(klass, "init")) {
        executor_.callMethodDecl(init, args, instanceValue);  // 반환값은 버린다.
    }
    return instanceValue;
}

MethodDeclareStatement* ClassRuntime::findMethod(const ClassDeclareStatement* klass, const std::string& name) const {
    for (const ClassDeclareStatement* k = klass; k != nullptr; k = resolveSuperclass(k)) {
        for (MethodDeclareStatement* method : k->methods) {
            if (method->name.origin == name) {
                return method;
            }
        }
    }
    return nullptr;
}

const ClassDeclareStatement* ClassRuntime::resolveSuperclass(const ClassDeclareStatement* klass) const {
    if (klass->superclass == nullptr) {
        return nullptr;
    }
    const IdentifierExpression* superclass = klass->superclass;
    // depth는 Checker가 클래스 선언을 검사하던 시점(항상 클래스가 선언된 스코프)의
    // 스코프 깊이를 캐싱한 것이다. 반면 resolveSuperclass는 메서드 호출 스택
    // 한가운데(예: Super.method() 안에서 다시 Super를 참조하는 다단계 상속, 또는
    // REPL에서 다른 줄에 걸쳐 호출되는 생성자)에서도 호출되므로, 그 시점의 실제
    // 스코프 깊이가 캐싱된 depth와 전혀 다를 수 있다 - lookupAt을 쓰면 엉뚱한
    // 스코프를 가리켜 "클래스가 아닙니다" 오류로 이어진다. 그래서 항상 전체
    // 스코프를 훑는 동적 조회(lookup)만 사용한다(InstanceOfExpression과 동일한 이유).
    auto value = executor_.environment_.lookup(superclass->name);
    if (!value || !value->isClass()) {
        throw ExecutorError("[line {}] '{}' is not a class.", superclass->getLine(), superclass->name);
    }
    return value->asClass();
}

Value ClassRuntime::callSuperMethod(FieldAccessExpression* fieldAccess, const std::vector<Expression*>& argExprs) {
    // Super.method(...): this는 그대로 유지한 채, 메서드 탐색 시작점만
    // this가 속한 클래스가 아니라 superclass로 강제 이동한다.
    auto thisValue = executor_.environment_.lookup("this");
    if (!thisValue || !thisValue->isInstance()) {
        throw ExecutorError("[line {}] cannot use 'Super' outside a class method.", fieldAccess->getLine());
    }
    const ClassDeclareStatement* startClass = resolveSuperclass(thisValue->asInstance()->klass);
    MethodDeclareStatement* method = startClass ? findMethod(startClass, fieldAccess->name.origin) : nullptr;
    if (!method) {
        throw ExecutorError("[line {}] method '{}' does not exist in superclass.", fieldAccess->getLine(), fieldAccess->name.origin);
    }
    std::vector<Value> args;
    args.reserve(argExprs.size());
    for (Expression* arg : argExprs) {
        args.push_back(executor_.evaluate(arg));
    }
    return executor_.callMethodDecl(method, args, *thisValue);
}

Value ClassRuntime::callInstanceMethod(const Value& object, FieldAccessExpression* fieldAccess,
    const std::vector<Expression*>& argExprs) {
    if (!object.isInstance()) {
        throw ExecutorError("[line {}] cannot call method on a non-instance.", fieldAccess->getLine());
    }
    auto& instance = object.asInstance();
    MethodDeclareStatement* method = findMethod(instance->klass, fieldAccess->name.origin);
    if (!method) {
        throw ExecutorError("[line {}] method '{}' does not exist.", fieldAccess->getLine(), fieldAccess->name.origin);
    }
    std::vector<Value> args;
    args.reserve(argExprs.size());
    for (Expression* arg : argExprs) {
        args.push_back(executor_.evaluate(arg));
    }
    return executor_.callMethodDecl(method, args, object);
}

bool ClassRuntime::isInstanceOf(const ClassDeclareStatement* klass, const ClassDeclareStatement* target) const {
    // 자기 자신뿐 아니라 superclass 체인 어딘가와 일치해도 true.
    for (const ClassDeclareStatement* k = klass; k != nullptr; k = resolveSuperclass(k)) {
        if (k == target) {
            return true;
        }
    }
    return false;
}
