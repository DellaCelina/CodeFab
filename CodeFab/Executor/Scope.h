#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "Value.h"

// A single block scope: a flat name -> Value table.
// Environment stacks Scopes to implement nested block scoping.
class Scope {
public:
    void define(const std::string& name, const Value& value);

    // Returns false if `name` is not defined in this scope.
    bool assign(const std::string& name, const Value& value);

    std::optional<Value> get(const std::string& name) const;

    // 이 스코프에 정의된 모든 (이름, 값)을 읽기 전용으로 노출한다. 디버그 모드의
    // inspect 명령(Architecture.md §9.3, Implement.md §5 "할 일 3")이 변수 목록을
    // 보여줄 때 쓴다.
    const std::unordered_map<std::string, Value>& variables() const { return variables_; }

private:
    std::unordered_map<std::string, Value> variables_;
};
