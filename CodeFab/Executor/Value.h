#pragma once

#include <memory>
#include <string>
#include <variant>

// AST 선언 노드를 함수/클래스 값으로 직접 재사용한다(Architecture.md §2.3) -
// 별도의 FunctionObject/ClassObject 래퍼를 두지 않는다. Value.h는 포인터만
// 다루므로 전방 선언만으로 충분하고, 이 노드들의 전체 정의는 필요한 .cpp에서
// "../Assembler/SyntaxTree.h"를 include해서 얻는다.
struct FunctionDeclareStatement;
struct ClassDeclareStatement;

// 클래스 인스턴스 / 고정 크기 배열. 정의는 InstanceValue.h / ArrayValue.h.
struct InstanceValue;
struct ArrayValue;

// import로 들여온 모듈의 export 스코프. Executor/Scope.h 참고.
class Scope;

// Runtime value produced/consumed by the Executor.
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

    // Truthiness for conditions: nil and false are falsy, everything else truthy.
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
