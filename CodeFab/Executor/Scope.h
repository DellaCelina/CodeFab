#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "Value.h"

// 블록 스코프 하나. 이름 → 값 테이블이며, Environment가 스택으로 쌓아 중첩 스코프를 구현한다.
class Scope {
public:
    void define(const std::string& name, const Value& value);

    // 이 스코프에 name이 없으면 false를 반환한다.
    bool assign(const std::string& name, const Value& value);

    std::optional<Value> get(const std::string& name) const;

    const std::unordered_map<std::string, Value>& variables() const { return variables_; }

private:
    std::unordered_map<std::string, Value> variables_;
};
