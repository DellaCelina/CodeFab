#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Scope.h"
#include "Value.h"

// 스코프 스택으로 변수 저장소를 관리한다.
// scopes_.front()가 전역, scopes_.back()이 가장 안쪽 스코프다.
// 미정의 변수/중복 선언은 throw하지 않고 bool/optional로 보고해 Executor가 처리하게 한다.
class Environment {
public:
    Environment();

    void pushScope();
    void popScope();

    void define(const std::string& name, const Value& value);

    // name이 정의된 스코프가 없으면 false를 반환한다.
    bool assign(const std::string& name, const Value& value);

    // name을 로컬→전역 순으로 탐색한다. 없으면 nullopt.
    std::optional<Value> lookup(const std::string& name) const;

    // Checker가 계산한 depth로 특정 스코프에 직접 접근한다(0 = 현재 스코프).
    std::optional<Value> lookupAt(int distance, const std::string& name) const;
    bool assignAt(int distance, const std::string& name, const Value& value);

    const std::vector<Scope>& scopes() const { return scopes_; }

private:
    std::vector<Scope> scopes_;
};

// 생성 시 스코프를 push하고, 예외가 발생해도 소멸 시 반드시 pop한다.
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
