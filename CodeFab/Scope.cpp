#include "Scope.h"

void Scope::define(const std::string& name, const Value& value) {
    variables_[name] = value;
}

bool Scope::assign(const std::string& name, const Value& value) {
    auto it = variables_.find(name);
    if (it == variables_.end()) {
        return false;
    }
    it->second = value;
    return true;
}

std::optional<Value> Scope::get(const std::string& name) const {
    auto it = variables_.find(name);
    if (it == variables_.end()) {
        return std::nullopt;
    }
    return it->second;
}
