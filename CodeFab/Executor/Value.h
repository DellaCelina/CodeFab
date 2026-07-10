#pragma once

#include <memory>
#include <string>
#include <variant>

// 함수/클래스는 AST 선언 노드를 그대로 값으로 재사용한다. 전방 선언으로 충분하다.
struct FunctionDeclareStatement;
struct ClassDeclareStatement;

// 클래스 인스턴스 / 고정 크기 배열. 정의는 InstanceValue.h / ArrayValue.h.
struct InstanceValue;
struct ArrayValue;

// import로 들여온 모듈의 export 스코프. Executor/Scope.h 참고.
class Scope;

// Executor가 생성하고 소비하는 런타임 값.
class Value {
public:
    enum class Type { Nil, Boolean, Number, String, Function, Class, Instance, Array, Module };

    Value();
    Value(bool boolean);
    Value(double number);
    Value(std::string text);
    Value(const FunctionDeclareStatement* function);
    Value(const ClassDeclareStatement* klass);
    Value(std::shared_ptr<InstanceValue> instance);
    Value(std::shared_ptr<ArrayValue> array);
    Value(std::shared_ptr<Scope> module);

    Type type() const { return type_; }

    bool isNil() const { return type_ == Type::Nil; }
    bool isBoolean() const { return type_ == Type::Boolean; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isFunction() const { return type_ == Type::Function; }
    bool isClass() const { return type_ == Type::Class; }
    bool isInstance() const { return type_ == Type::Instance; }
    bool isArray() const { return type_ == Type::Array; }
    bool isModule() const { return type_ == Type::Module; }

    bool asBoolean() const;
    double asNumber() const;
    const std::string& asString() const;
    const FunctionDeclareStatement* asFunction() const;
    const ClassDeclareStatement* asClass() const;
    const std::shared_ptr<InstanceValue>& asInstance() const;
    const std::shared_ptr<ArrayValue>& asArray() const;
    const std::shared_ptr<Scope>& asModule() const;

    // nil과 false는 거짓, 나머지는 참.
    bool isTruthy() const;

    std::string toString() const;
    const char* typeName() const;

    bool operator==(const Value& other) const;

private:
    Type type_;
    std::variant<std::monostate, bool, double, std::string,
                 const FunctionDeclareStatement*,
                 const ClassDeclareStatement*,
                 std::shared_ptr<InstanceValue>,
                 std::shared_ptr<ArrayValue>,
                 std::shared_ptr<Scope>>
        data_;
};
