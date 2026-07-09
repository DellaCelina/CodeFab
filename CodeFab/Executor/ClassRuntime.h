#pragma once

#include <vector>

#include "../Assembler/SyntaxTree.h"
#include "Value.h"

class Executor;

// 클래스 인스턴스화와 메서드 탐색/호출을 전담한다(Executor의 God Object 분리 -
// 전체리팩토링리스트 #3). superclass 체인 탐색(findMethod/resolveSuperclass)이
// instantiate/callSuperMethod/callInstanceMethod/isInstanceOf 네 곳에서
// 공유되므로 한 클래스에 모아둔다. Executor의 private 멤버(environment_,
// invoke 계열)에 접근해야 해서 Executor가 friend로 열어준다.
class ClassRuntime {
public:
    explicit ClassRuntime(Executor& executor);

    // klass를 인스턴스화한다: 필드 저장소를 만들고, init 메서드가 있으면
    // 호출한다(반환값은 버리고 항상 새 인스턴스를 반환).
    Value instantiate(const ClassDeclareStatement* klass, const std::vector<Value>& args);

    // Super.method(...): this는 그대로 유지한 채, 메서드 탐색 시작점만
    // this가 속한 클래스가 아니라 superclass로 강제 이동한다.
    Value callSuperMethod(FieldAccessExpression* fieldAccess, const std::vector<Expression*>& argExprs);

    // object.method(...) - object가 인스턴스일 때의 일반 메서드 호출.
    Value callInstanceMethod(const Value& object, FieldAccessExpression* fieldAccess,
        const std::vector<Expression*>& argExprs);

    // instanceof 평가: klass부터 superclass 체인을 따라 올라가며 target과
    // 일치하는 게 있는지 확인한다(자기 자신이어도 true).
    bool isInstanceOf(const ClassDeclareStatement* klass, const ClassDeclareStatement* target) const;

private:
    // klass부터 시작해 superclass 체인을 따라 올라가며 name과 일치하는 메서드를
    // 찾는다(자식 클래스부터 먼저 찾으므로 오버라이딩이 자연히 해결됨). 없으면
    // nullptr.
    MethodDeclareStatement* findMethod(const ClassDeclareStatement* klass, const std::string& name) const;

    // klass->superclass(IdentifierExpression*)를 실제 ClassDeclareStatement*로
    // 조회한다. superclass가 없으면 nullptr.
    const ClassDeclareStatement* resolveSuperclass(const ClassDeclareStatement* klass) const;

    Executor& executor_;
};
