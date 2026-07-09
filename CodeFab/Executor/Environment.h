#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Scope.h"
#include "Value.h"

// Manages the stack of Scopes that backs variable storage.
// scopes_.front() is the global scope; scopes_.back() is the innermost
// (current) scope. Lookup/assign walk from innermost to global.
//
// Deliberately does not throw on undefined-variable/redeclaration itself:
// it reports success/failure via bool/optional so the Executor can raise
// an ExecutorError.
class Environment {
public:
    Environment();

    void pushScope();
    void popScope();

    // Defines `name` in the current (innermost) scope.
    void define(const std::string& name, const Value& value);

    // Assigns to the nearest scope (local -> global) that already defines
    // `name`. Returns false if no scope defines it.
    bool assign(const std::string& name, const Value& value);

    // Looks up `name` from local -> global. Returns nullopt if not found.
    std::optional<Value> lookup(const std::string& name) const;

    // 정적 바인딩(실행전 최적화, Architecture.md §6.1) 전용 접근자. `distance`
    // 단계만큼 안쪽(back)에서 바깥쪽으로 건너뛴 정확히 그 스코프에서만
    // 조회/대입한다(0 = 현재 스코프). Checker의 Resolver가 IdentifierExpression::depth
    // 에 채워 넣은 값을 그대로 넘기면 된다 - 스코프 깊이와 무관한 상수 시간
    // 접근이라 lookup()/assign()의 매번 전체 스코프를 훑는 동적 조회보다 빠르다.
    std::optional<Value> lookupAt(int distance, const std::string& name) const;
    bool assignAt(int distance, const std::string& name, const Value& value);

    // scopes_.front()가 전역, scopes_.back()이 가장 안쪽 스코프인 순서 그대로
    // 읽기 전용으로 노출한다. 디버그 모드의 inspect 명령이 전체 변수 목록을
    // 보여줄 때 쓴다(Architecture.md §9.3, Implement.md §5 "할 일 3").
    const std::vector<Scope>& scopes() const { return scopes_; }

private:
    std::vector<Scope> scopes_;
};
