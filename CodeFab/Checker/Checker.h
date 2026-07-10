#pragma once
#include <string>
#include <unordered_set>
#include <vector>

#include "../Assembler/SyntaxTree.h"
#include "CheckerInterface.h"

using namespace std;

// Visitor 패턴으로 디스패치한다. visit() 누락은 컴파일 오류로 잡힌다.
class Checker : public CheckerInterface, public SyntaxNodeVisitor {

public:
    // scopes는 REPL 세션 내내 유지된다(Executor의 Environment와 동일).
    Checker();

    // 의미 오류 발견 시 CheckerError를 throw, 통과하면 정상 반환.
    void check(SyntaxTree& tree) override;

    // SyntaxNodeVisitor: 노드 하나당 visit() 하나.
    void visit(IdentifierExpression& node) override;
    void visit(PrintStatement& node) override;
    void visit(ExpressionStatement& node) override;
    void visit(DeclareStatement& node) override;
    void visit(BlockStatement& node) override;
    void visit(IfStatement& node) override;
    void visit(ForStatement& node) override;
    void visit(NumberExpression& node) override;
    void visit(StringExpression& node) override;
    void visit(BooleanExpression& node) override;
    void visit(AddExpression& node) override;
    void visit(MultExpression& node) override;
    void visit(SubExpression& node) override;
    void visit(DivideExpression& node) override;
    void visit(ModExpression& node) override;
    void visit(AndExpression& node) override;
    void visit(OrExpression& node) override;
    void visit(EqualExpression& node) override;
    void visit(NotEqualExpression& node) override;
    void visit(LessExpression& node) override;
    void visit(LessEqualExpression& node) override;
    void visit(GreaterExpression& node) override;
    void visit(GreaterEqualExpression& node) override;
    void visit(AssignExpression& node) override;
    void visit(NegativeExpression& node) override;
    void visit(NotExpression& node) override;
    void visit(CallExpression& node) override;
    void visit(FieldAccessExpression& node) override;
    void visit(ThisExpression& node) override;
    void visit(SuperExpression& node) override;
    void visit(ArrayExpression& node) override;
    void visit(IndexExpression& node) override;
    void visit(InstanceOfExpression& node) override;
    void visit(FunctionDeclareStatement& node) override;
    // MethodDeclareStatement는 클래스 바디 전용 선언이라 accept()로 직접 방문되지
    // 않는다(visit(ClassDeclareStatement&)가 checkFunctionBody로 바로 처리) -
    // Executor의 동일 노드 처리와 같은 이유.
    void visit(MethodDeclareStatement& node) override;
    void visit(ReturnStatement& node) override;
    void visit(ClassDeclareStatement& node) override;
    void visit(ImportStatement& node) override;

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

    // accept()/visit() 이중 디스패치 진입점. nullptr은 호출부가 미리 걸러낸다
    // (IfStatement::elseBranch, ReturnStatement::value처럼 nullable한 필드가
    // 대상일 때) - Executor의 execute()/evaluate()와 동일한 관례.
    void checkStatement(Statement* stmt);
    void checkExpression(Expression* expr);

    // BinaryExpression 하위 12개 타입이 전부 "양쪽을 재귀 검사"만 동일하게 하므로
    // 공유 헬퍼로 둔다.
    void checkBinary(BinaryExpression& bin);

    // FunctionDeclareStatement/MethodDeclareStatement가 필드 모양이 같아 공용으로 처리한다.
    void checkFunctionBody(const string& name, const vector<Token>& params,
        const vector<Statement*>& body, int line, bool isMethod, bool isInit);

    // 정적 바인딩: 몇 단계 위 스코프에서 선언됐는지 세어 id.depth에 기록한다.
    void resolveIdentifier(IdentifierExpression& id) const;
};
