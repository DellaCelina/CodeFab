#pragma once

#include <vector>

#include "../Assembler/SyntaxTree.h"
#include "Value.h"

class Executor;

// import 실행과 모듈 멤버 호출(alias.add(...))을 전담한다(Executor의
// God Object 분리 - 전체리팩토링리스트 #3). Executor의 private 멤버
// (environment_, execute)에 접근해야 해서 Executor가 friend로 열어준다.
class ModuleRuntime {
public:
    explicit ModuleRuntime(Executor& executor);

    // ImportStatement를 실행해 alias를 바깥(호출 시점) 스코프에 등록한다.
    void runImport(ImportStatement& node);

    // fieldAccess->object가 모듈일 때(alias.add(...)) 호출한다.
    Value callMember(const Value& moduleValue, FieldAccessExpression* fieldAccess,
        const std::vector<Expression*>& argExprs);

private:
    Executor& executor_;
};
