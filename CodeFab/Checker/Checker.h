#pragma once
#include <functional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../Assembler/SyntaxTree.h"
#include "CheckerInterface.h"

using namespace std;


class Checker : public CheckerInterface {

public:
    // scopes는 REPL 세션 내내 유지된다(Executor의 Environment와 동일).
    Checker();

    // 의미 오류 발견 시 CheckerError를 throw, 통과하면 정상 반환.
    void check(SyntaxTree& tree) override;

private:
    vector<unordered_set<string>> scopes; // 블록 진입 시 push, 종료 시 pop
    vector<unordered_set<string>> classNames_; // scopes와 나란히 push/pop, Class로 선언된 이름만
    string currentlyDeclaring; // 자기 참조 검사용: 지금 초기화식을 검사 중인 변수 이름

    int functionDepth = 0;     // 0이면 함수/메서드 밖 - return 검사용
    int classMethodDepth = 0;  // 0이면 클래스 메서드 밖 - This 검사용
    int forDepth = 0;          // 0이면 for 문 밖 - import 금지 검사용
    bool inInitMethod = false; // 지금 검사 중인 메서드가 init인지 - return 값 금지 검사용
    bool hasSuperclass_ = false; // 지금 검사 중인 메서드가 상속 클래스 소속인지 - Super 검사용

    void enterScope();
    void exitScope();
    bool isDeclaredInCurrentScope(const string& name) const;
    bool isDeclaredInAnyScope(const string& name) const;
    void declare(const string& name);
    void declareClass(const string& name);
    bool isClassDeclaredInAnyScope(const string& name) const;

    [[noreturn]] void reportError(int line, const string& message);

    // SyntaxNode에 accept()가 추가됐고(TODO.md #11, Visitor 패턴 적용 결정),
    // Executor는 이미 SyntaxNodeVisitor로 전환됐다(ImplementTodo.md §4 할 일 4).
    // Checker는 이번 라운드 리팩토링 범위가 아니라 여전히 type_index 맵으로 타입
    // 분기한다 - Executor 전환 방식(람다 -> visit() 오버라이드)을 참고해 별도로
    // 진행한다.
    void registerDefaultHandlers();

    // 핸들러 없는 타입(리터럴 등)은 조용히 지나간다.
    void checkStatement(Statement* stmt);
    void checkExpression(Expression* expr);

    void checkBlock(BlockStatement* block);
    void checkDeclare(DeclareStatement* decl);
    void checkPrint(PrintStatement* stmt);
    void checkIf(IfStatement* ifStmt);
    void checkFor(ForStatement* forStmt);
    void checkIdentifier(IdentifierExpression* id);
    void checkBinary(BinaryExpression* bin);
    void checkSuper(SuperExpression* superExpr);

    void checkFunctionDeclare(FunctionDeclareStatement* funcDecl);

    // FunctionDeclareStatement/MethodDeclareStatement가 필드 모양이 같아 공용으로 처리한다.
    void checkFunctionBody(const string& name, const vector<Token>& params,
        const vector<Statement*>& body, int line, bool isMethod, bool isInit);
    void checkClass(ClassDeclareStatement* classDecl);
    void checkReturn(ReturnStatement* ret);
    void checkImport(ImportStatement* importStmt);
    void checkThis(ThisExpression* thisExpr);

    // 정적 바인딩: 몇 단계 위 스코프에서 선언됐는지 세어 id->depth에 기록한다.
    void resolveIdentifier(IdentifierExpression* id) const;

    unordered_map<type_index, function<void(Statement*)>> statementHandlers_;
    unordered_map<type_index, function<void(Expression*)>> expressionHandlers_;
};
