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

private:
    std::vector<Scope> scopes_;
};
