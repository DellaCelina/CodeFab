#pragma once
#include <string>
#include <vector>
#include <unordered_set>

#include "../Assembler/SyntaxTree.h"
#include "../Executor/ExecuteInterface.h"
#include "CheckerInterface.h"

using namespace std;

class Checker : public CheckerInterface {

public:
    // scopes는 REPL 세션 내내 유지된다(Executor의 Environment와 동일).
    // executor는 ConstantFolder용으로 받아두었지만 아직 사용하지 않는다.
    explicit Checker(ExecuteInterface& executor);

    // 의미 오류 발견 시 CheckerError를 throw, 통과하면 true 반환.
    bool check(SyntaxTree& tree) override;

private:
    ExecuteInterface& executor_;
    vector<unordered_set<string>> scopes; // 블록 진입 시 push, 종료 시 pop
    string currentlyDeclaring; // 자기 참조 검사용: 지금 초기화식을 검사 중인 변수 이름

    int functionDepth = 0;     // 0이면 함수/메서드 밖 - return 검사용
    int classMethodDepth = 0;  // 0이면 클래스 메서드 밖 - This 검사용
    int forDepth = 0;          // 0이면 for 문 밖 - import 금지 검사용
    bool inInitMethod = false; // 지금 검사 중인 메서드가 init인지 - return 값 금지 검사용

    void enterScope();
    void exitScope();
    bool isDeclaredInCurrentScope(const string& name) const;
    bool isDeclaredInAnyScope(const string& name) const;
    void declare(const string& name);

    [[noreturn]] void reportError(int line, const string& message);

    // SyntaxNode에 accept()가 없어 dynamic_cast로 타입 분기한다.
    void checkStatement(Statement* stmt);
    void checkExpression(Expression* expr);

    void checkBlock(BlockStatement* block);
    void checkDeclare(DeclareStatement* decl);
    void checkPrint(PrintStatement* stmt);
    void checkIf(IfStatement* ifStmt);
    void checkFor(ForStatement* forStmt);
    void checkIdentifier(IdentifierExpression* id);
    void checkBinary(BinaryExpression* bin);

    // FunctionDeclareStatement/MethodDeclareStatement가 필드 모양이 같아 공용으로 처리한다.
    void checkFunctionBody(const string& name, const vector<Token>& params,
        const vector<Statement*>& body, int line, bool isMethod, bool isInit);
    void checkClass(ClassDeclareStatement* classDecl);
    void checkReturn(ReturnStatement* ret);
    void checkImport(ImportStatement* importStmt);
    void checkThis(ThisExpression* thisExpr);

    // 정적 바인딩: 몇 단계 위 스코프에서 선언됐는지 세어 id->depth에 기록한다.
    void resolveIdentifier(IdentifierExpression* id) const;
};
