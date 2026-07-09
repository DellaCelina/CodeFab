#include "ModuleRuntime.h"

#include "Executor.h"

namespace {

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
    throw ExecutorError("unsupported declaration type in import.");
}

}  // namespace

ModuleRuntime::ModuleRuntime(Executor& executor) : executor_(executor) {}

void ModuleRuntime::runImport(ImportStatement& node) {
    auto moduleScope = std::make_shared<Scope>();

    {
        ScopeGuard guard(executor_.environment_);  // declarations 실행용 임시 프레임.
        for (Statement* decl : node.declarations) {
            executor_.execute(decl);
            std::string name = declaredNameOf(decl);
            moduleScope->define(name, *executor_.environment_.lookup(name));
        }
    }  // 임시 프레임은 여기서 pop된다 - alias는 바깥(호출 시점) 스코프에 등록한다.

    executor_.environment_.define(node.alias.origin, Value(moduleScope));
}

Value ModuleRuntime::callMember(const Value& moduleValue, FieldAccessExpression* fieldAccess,
    const std::vector<Expression*>& argExprs) {
    // alias.add(...): 모듈 스코프에서 이름을 찾아 함수처럼 호출한다.
    auto member = moduleValue.asModule()->get(fieldAccess->name.origin);
    if (!member || !member->isFunction()) {
        throw ExecutorError("[line {}] '{}' is not a function in module.", fieldAccess->getLine(), fieldAccess->name.origin);
    }
    std::vector<Value> args;
    args.reserve(argExprs.size());
    for (Expression* arg : argExprs) {
        args.push_back(executor_.evaluate(arg));
    }
    return executor_.callFunction(member->asFunction(), args);
}
