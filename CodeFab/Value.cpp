#include "Value.h"

#include <sstream>

Value::Value() : type_(Type::Nil), data_(std::monostate{}) {}

Value::Value(bool boolean) : type_(Type::Boolean), data_(boolean) {}

Value::Value(double number) : type_(Type::Number), data_(number) {}

Value::Value(std::string text) : type_(Type::String), data_(std::move(text)) {}

bool Value::asBoolean() const {
    return std::get<bool>(data_);
}

double Value::asNumber() const {
    return std::get<double>(data_);
}

const std::string& Value::asString() const {
    return std::get<std::string>(data_);
}

bool Value::isTruthy() const {
    switch (type_) {
        case Type::Nil:
            return false;
        case Type::Boolean:
            return asBoolean();
        default:
            return true;
    }
}

std::string Value::toString() const {
    switch (type_) {
        case Type::Nil:
            return "nil";
        case Type::Boolean:
            return asBoolean() ? "true" : "false";
        case Type::Number: {
            std::ostringstream out;
            out << asNumber();
            return out.str();
        }
        case Type::String:
            return asString();
    }
    return "nil";
}

const char* Value::typeName() const {
    switch (type_) {
        case Type::Nil:
            return "nil";
        case Type::Boolean:
            return "boolean";
        case Type::Number:
            return "number";
        case Type::String:
            return "string";
    }
    return "nil";
}

bool Value::operator==(const Value& other) const {
    if (type_ != other.type_) {
        return false;
    }
    switch (type_) {
        case Type::Nil:
            return true;
        case Type::Boolean:
            return asBoolean() == other.asBoolean();
        case Type::Number:
            return asNumber() == other.asNumber();
        case Type::String:
            return asString() == other.asString();
    }
    return false;
}
