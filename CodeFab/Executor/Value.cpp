#include "Value.h"

#include <sstream>

#include "ArrayValue.h"
#include "InstanceValue.h"
#include "Scope.h"
#include "../Assembler/SyntaxTree.h"

Value::Value() : type_(Type::Nil), data_(std::monostate{}) {}

Value::Value(bool boolean) : type_(Type::Boolean), data_(boolean) {}

Value::Value(double number) : type_(Type::Number), data_(number) {}

Value::Value(std::string text) : type_(Type::String), data_(std::move(text)) {}

Value::Value(const FunctionDeclareStatement* function) : type_(Type::Function), data_(function) {}

Value::Value(const ClassDeclareStatement* klass) : type_(Type::Class), data_(klass) {}

Value::Value(std::shared_ptr<InstanceValue> instance) : type_(Type::Instance), data_(std::move(instance)) {}

Value::Value(std::shared_ptr<ArrayValue> array) : type_(Type::Array), data_(std::move(array)) {}

Value::Value(std::shared_ptr<Scope> module) : type_(Type::Module), data_(std::move(module)) {}

bool Value::asBoolean() const {
    return std::get<bool>(data_);
}

double Value::asNumber() const {
    return std::get<double>(data_);
}

const std::string& Value::asString() const {
    return std::get<std::string>(data_);
}

const FunctionDeclareStatement* Value::asFunction() const {
    return std::get<const FunctionDeclareStatement*>(data_);
}

const ClassDeclareStatement* Value::asClass() const {
    return std::get<const ClassDeclareStatement*>(data_);
}

const std::shared_ptr<InstanceValue>& Value::asInstance() const {
    return std::get<std::shared_ptr<InstanceValue>>(data_);
}

const std::shared_ptr<ArrayValue>& Value::asArray() const {
    return std::get<std::shared_ptr<ArrayValue>>(data_);
}

const std::shared_ptr<Scope>& Value::asModule() const {
    return std::get<std::shared_ptr<Scope>>(data_);
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
        case Type::Function:
            return "<fn " + asFunction()->name.origin + ">";
        case Type::Class:
            return "<class " + asClass()->name.origin + ">";
        case Type::Instance:
            return "<instance of " + asInstance()->klass->name.origin + ">";
        case Type::Array:
            return "<array[" + std::to_string(asArray()->items.size()) + "]>";
        case Type::Module:
            return "<module>";
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
        case Type::Function:
            return "function";
        case Type::Class:
            return "class";
        case Type::Instance:
            return "instance";
        case Type::Array:
            return "array";
        case Type::Module:
            return "module";
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
        case Type::Function:
            return asFunction() == other.asFunction();
        case Type::Class:
            return asClass() == other.asClass();
        case Type::Instance:
            return asInstance() == other.asInstance();
        case Type::Array:
            return asArray() == other.asArray();
        case Type::Module:
            return asModule() == other.asModule();
    }
    return false;
}
