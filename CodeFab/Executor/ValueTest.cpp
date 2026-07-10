#include <memory>
#include <vector>

#include "gtest/gtest.h"

#include "ArrayValue.h"
#include "InstanceValue.h"
#include "Scope.h"
#include "Value.h"
#include "../Assembler/SyntaxTree.h"

// Value는 ExecutorTest.cpp 등 다른 테스트 파일에서 Number/String 타입 위주로
// 간접적으로 쓰이지만, isTruthy()/toString()/typeName()/operator==()가 모든
// Type(특히 Function/Class/Instance/Array/Module)에 대해 각각 올바른 분기를
// 타는지 직접 검증하는 전용 테스트는 없었다. 이 파일은 Value 자체의 단위
// 테스트다(Executor 없이 Value만 손으로 구성).

namespace {

FunctionDeclareStatement makeFunctionDecl(const std::string& name) {
    return FunctionDeclareStatement({}, Token{ TokenType::IDENTIFIER, name, 0 }, {}, {});
}

ClassDeclareStatement makeClassDecl(const std::string& name) {
    return ClassDeclareStatement({}, Token{ TokenType::IDENTIFIER, name, 0 }, {});
}

}  // namespace

TEST(ValueTest, Nil_IsFalsyAndReportsNilTypeNameAndToString) {
    Value nil;
    EXPECT_TRUE(nil.isNil());
    EXPECT_FALSE(nil.isTruthy());
    EXPECT_EQ(nil.toString(), "nil");
    EXPECT_STREQ(nil.typeName(), "nil");
}

TEST(ValueTest, Boolean_TruthinessMatchesValue) {
    Value t(true);
    Value f(false);
    EXPECT_TRUE(t.isTruthy());
    EXPECT_FALSE(f.isTruthy());
    EXPECT_EQ(t.toString(), "true");
    EXPECT_EQ(f.toString(), "false");
    EXPECT_STREQ(t.typeName(), "boolean");
}

TEST(ValueTest, NonNilNonBooleanTypes_AreAlwaysTruthy) {
    // isTruthy()의 default 분기(Nil/Boolean 외 전부 true)를 Number/String
    // 각각으로 확인한다.
    EXPECT_TRUE(Value(0.0).isTruthy());
    EXPECT_TRUE(Value(std::string("")).isTruthy());
}

TEST(ValueTest, Function_ToStringTypeNameAndEquality) {
    FunctionDeclareStatement decl = makeFunctionDecl("square");
    FunctionDeclareStatement other = makeFunctionDecl("cube");
    Value fn(&decl);
    Value sameFn(&decl);
    Value otherFn(&other);

    EXPECT_TRUE(fn.isTruthy());
    EXPECT_EQ(fn.toString(), "<fn square>");
    EXPECT_STREQ(fn.typeName(), "function");
    EXPECT_TRUE(fn == sameFn);
    EXPECT_FALSE(fn == otherFn);
}

TEST(ValueTest, Class_ToStringTypeNameAndEquality) {
    ClassDeclareStatement decl = makeClassDecl("Robot");
    ClassDeclareStatement other = makeClassDecl("Cat");
    Value klass(&decl);
    Value sameKlass(&decl);
    Value otherKlass(&other);

    EXPECT_TRUE(klass.isTruthy());
    EXPECT_EQ(klass.toString(), "<class Robot>");
    EXPECT_STREQ(klass.typeName(), "class");
    EXPECT_TRUE(klass == sameKlass);
    EXPECT_FALSE(klass == otherKlass);
}

TEST(ValueTest, Instance_ToStringTypeNameAndEquality) {
    ClassDeclareStatement decl = makeClassDecl("Robot");
    auto instance = std::make_shared<InstanceValue>();
    instance->klass = &decl;
    instance->fields = std::make_shared<Scope>();
    auto otherInstance = std::make_shared<InstanceValue>();
    otherInstance->klass = &decl;
    otherInstance->fields = std::make_shared<Scope>();

    Value instanceValue(instance);
    Value sameInstanceValue(instance);
    Value otherInstanceValue(otherInstance);

    EXPECT_TRUE(instanceValue.isTruthy());
    EXPECT_EQ(instanceValue.toString(), "<instance of Robot>");
    EXPECT_STREQ(instanceValue.typeName(), "instance");
    EXPECT_TRUE(instanceValue == sameInstanceValue);
    EXPECT_FALSE(instanceValue == otherInstanceValue);
}

TEST(ValueTest, Array_ToStringTypeNameAndEquality) {
    auto array = std::make_shared<ArrayValue>();
    array->items.resize(3);
    auto otherArray = std::make_shared<ArrayValue>();

    Value arrayValue(array);
    Value sameArrayValue(array);
    Value otherArrayValue(otherArray);

    EXPECT_TRUE(arrayValue.isTruthy());
    EXPECT_EQ(arrayValue.toString(), "<array[3]>");
    EXPECT_STREQ(arrayValue.typeName(), "array");
    EXPECT_TRUE(arrayValue == sameArrayValue);
    EXPECT_FALSE(arrayValue == otherArrayValue);
}

TEST(ValueTest, Module_ToStringTypeNameAndEquality) {
    auto scope = std::make_shared<Scope>();
    auto otherScope = std::make_shared<Scope>();

    Value moduleValue(scope);
    Value sameModuleValue(scope);
    Value otherModuleValue(otherScope);

    EXPECT_TRUE(moduleValue.isTruthy());
    EXPECT_EQ(moduleValue.toString(), "<module>");
    EXPECT_STREQ(moduleValue.typeName(), "module");
    EXPECT_TRUE(moduleValue == sameModuleValue);
    EXPECT_FALSE(moduleValue == otherModuleValue);
}

TEST(ValueTest, String_Equality) {
    Value a(std::string("hi"));
    Value sameA(std::string("hi"));
    Value b(std::string("bye"));

    EXPECT_TRUE(a == sameA);
    EXPECT_FALSE(a == b);
}

TEST(ValueTest, Nil_AlwaysEqualToAnotherNil) {
    EXPECT_TRUE(Value() == Value());
}

TEST(ValueTest, Boolean_EqualityComparesUnderlyingValue) {
    EXPECT_TRUE(Value(true) == Value(true));
    EXPECT_FALSE(Value(true) == Value(false));
}

TEST(ValueTest, DifferentTypes_AreNeverEqualEvenWithSimilarRepresentation) {
    // operator==()의 첫 분기(type_ != other.type_ -> false)를 확인한다 - 값이
    // "비슷해 보여도"(1.0 vs true) 타입이 다르면 항상 false여야 한다.
    EXPECT_FALSE(Value(1.0) == Value(true));
    EXPECT_FALSE(Value() == Value(0.0));
}
