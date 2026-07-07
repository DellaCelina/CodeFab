#pragma once
#include <string>
#include <vector>
#include <unordered_set>

#include "SyntaxTree.h"

using namespace std;

struct CheckError {
    string message;

    bool operator==(const CheckError& other) const {
        return message == other.message;
    }
};

struct CheckResult {
    bool passed;
    vector<CheckError> errors;

    bool operator==(const CheckResult& other) const {
        return passed == other.passed && errors == other.errors;
    }
};

class Checker {

public:
    // syntax tree 전체를 DFS(재귀호출)로 순회하며 의미상 오류를 검사하는 진입점.
    CheckResult check(SyntaxTree& tree);

private:
    vector<unordered_set<string>> scopes; // 블록 검사를 위한 scope. 블록 진입 push, 블록 종료 pop
    vector<CheckError> errors; // DFS 순회중 error 저장
    string currentlyDeclaring; // 자기 참조 검사용 상태값. 현재 checking 중인 초기화 변수

    void enterScope();
    void exitScope();
    bool isDeclaredInCurrentScope(const string& name) const;
    bool isDeclaredInAnyScope(const string& name) const;
    void declare(const string& name);
    void reportError(int line, const string& message);

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
