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

private:
    std::unordered_map<std::string, Value> variables_;
};
