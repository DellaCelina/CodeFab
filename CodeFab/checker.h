#pragma once
#include <string>
#include <vector>
#include <unordered_set>

#include "SyntaxTree.h"
#include "CheckerInterface.h"
#include "ShellErrors.h"

using namespace std;

class Checker : public CheckerInterface {

public:
    // CheckerInterface 구현체. 의미 오류를 찾으면 CheckerError(line, message)를 throw한다.
    // 통과하면 true를 반환한다.
    bool check(SyntaxTree& tree) override;

private:
    vector<unordered_set<string>> scopes; // 블록 검사를 위한 scope. 블록 진입 push, 블록 종료 pop
    string currentlyDeclaring; // 자기 참조 검사용 상태값. 현재 checking 중인 초기화 변수

    void enterScope();
    void exitScope();
    bool isDeclaredInCurrentScope(const string& name) const;
    bool isDeclaredInAnyScope(const string& name) const;
    void declare(const string& name);

    // 의미 오류 발견 시 CheckerError를 throw하고 반환하지 않는다.
    [[noreturn]] void reportError(int line, const string& message);

    // DFS
    // SyntaxNode에 accept()가 없어(Visitor 패턴 적용 불가) dynamic_cast로 실제 타입을
    // 판별해서 분기하는 방식(RTTI 기반)을 사용
    // 새로운 노드 타입 추가되면 두 함수의 분기(else if) 필요
    void checkStatement(Statement* stmt);
    void checkExpression(Expression* expr);

    // Node type checker func.
    void checkBlock(BlockStatement* block);
    void checkDeclare(DeclareStatement* decl);
    void checkPrint(PrintStatement* stmt);
    void checkIdentifier(IdentifierExpression* id);
    void checkBinary(BinaryExpression* bin);
};
