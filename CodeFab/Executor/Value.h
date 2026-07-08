#pragma once

#include <string>
#include <variant>

// Runtime value produced/consumed by the Executor.
class Value {
public:
    enum class Type { Nil, Boolean, Number, String };

    Value();
    Value(bool boolean);
    Value(double number);
    Value(std::string text);

    Type type() const { return type_; }

    bool isNil() const { return type_ == Type::Nil; }
    bool isBoolean() const { return type_ == Type::Boolean; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }

    bool asBoolean() const;
    double asNumber() const;
    const std::string& asString() const;

    // Truthiness for conditions: nil and false are falsy, everything else truthy.
    bool isTruthy() const;

    std::string toString() const;
    const char* typeName() const;

    bool operator==(const Value& other) const;

private:
    Type type_;
    std::variant<std::monostate, bool, double, std::string> data_;
};
