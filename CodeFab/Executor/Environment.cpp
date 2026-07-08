#include "Environment.h"

#include <stdexcept>

Environment::Environment() {
    scopes_.emplace_back();  // global scope
}

void Environment::pushScope() {
    scopes_.emplace_back();
}

void Environment::popScope() {
    if (scopes_.size() <= 1) {
        throw std::logic_error("Cannot pop the global scope.");
    }
    scopes_.pop_back();
}

void Environment::define(const std::string& name, const Value& value) {
    scopes_.back().define(name, value);
}

bool Environment::assign(const std::string& name, const Value& value) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (it->assign(name, value)) {
            return true;
        }
    }
    return false;
}

std::optional<Value> Environment::lookup(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (auto value = it->get(name)) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<Value> Environment::lookupAt(int distance, const std::string& name) const {
    size_t index = scopes_.size() - 1 - static_cast<size_t>(distance);
    return scopes_[index].get(name);
}

bool Environment::assignAt(int distance, const std::string& name, const Value& value) {
    size_t index = scopes_.size() - 1 - static_cast<size_t>(distance);
    return scopes_[index].assign(name, value);
}
