#include "Checker.h"

// scope func.

void Checker::enterScope() {
    scopes.emplace_back();
}

void Checker::exitScope() {
    scopes.pop_back();
}

bool Checker::isDeclaredInCurrentScope(const string& name) const {
    // "같은 블록 내 중복"만 검사하므로 스택의 맨 위(top) 스코프만 확인한다.
    return !scopes.empty() && scopes.back().count(name) > 0;
}

bool Checker::isDeclaredInAnyScope(const string& name) const {
    // 바깥쪽 스코프까지 전부 확인 (변수 사용 시점의 유효성 검사용).
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if (it->count(name) > 0) {
            return true;
        }
    }
    return false;
}

void Checker::declare(const string& name) {
    if (!scopes.empty()) {
        scopes.back().insert(name);
    }
}

void Checker::reportError(int line, const string& message) {
    // error 발생 line 표시, 향후 error 포맷은 수정 가능
    errors.push_back(CheckError{ "[" + to_string(line) + "번째 줄] " + message });
}

// ---------------------------------------------------------------------------
// DFS  (RTTI - accept()가 없어서 dynamic_cast로 타입 분기)
// ---------------------------------------------------------------------------

void Checker::checkStatement(Statement* stmt) {
    if (stmt == nullptr) {
        return;
    }

    if (auto* block = dynamic_cast<BlockStatement*>(stmt)) {
        checkBlock(block);
    }
    else if (auto* decl = dynamic_cast<VarDeclStatement*>(stmt)) {
        checkVarDecl(decl);
    }
    else if (auto* print = dynamic_cast<PrintStatement*>(stmt)) {
        checkPrint(print);
    }
    // ASSUMPTION: 여기 걸리지 않는 새 Statement 타입이 assembler 쪽에 추가되면
    // 분기 추가 필요
}

void Checker::checkExpression(Expression* expr) {
    if (expr == nullptr) {
        return;
    }

    if (auto* id = dynamic_cast<IdentifierExpression*>(expr)) {
        checkIdentifier(id);
    }
    else if (auto* bin = dynamic_cast<BinaryExpression*>(expr)) {
        // AddExpression, MultExpression 모두 BinaryExpression을 상속하므로
        // 여기서 한 번에 처리된다 (왼쪽/오른쪽 자식을 재귀적으로 검사).
        checkBinary(bin);
    }
    // LiteralExpression(숫자 리터럴)은 그 자체로는 검사할 의미 규칙이 없어 별도 분기가 없다.
}

// ---------------------------------------------------------------------------
// 노드별 검사 로직
// ---------------------------------------------------------------------------

void Checker::checkBlock(BlockStatement* block) {
    enterScope();
    for (Statement* stmt : block->statements) {
        checkStatement(stmt);
    }
    exitScope();
}

void Checker::checkVarDecl(VarDeclStatement* decl) {
    // [검사 1] 변수 중복 선언: 같은 스코프에 이미 같은 이름이 있으면 에러.
    // (p.72: "{ var a = 11; var a = 12; }" -> 2번째 var a 에서 에러)
    if (isDeclaredInCurrentScope(decl->name)) {
        reportError(decl->getLine(),
            "'" + decl->name + "'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.");
        // 에러가 나도 계속 진행한다 (fail-fast가 아니라 에러를 최대한 누적해서 보여주는 방식).
    }

    // [검사 2] 선언 시 자기 참조: var a = a + 1; 처럼 초기화식 안에서 자기 자신을 읽으면 에러.
    // (p.73: "{ var a = a + 1; }")
    //
    // 핵심 아이디어: 초기화식을 검사하는 "동안에는" 아직 심볼 테이블에 변수를 등록하지 않는다.
    // 대신 지금 선언 중인 이름을 currentlyDeclaring에 기억해두고,
    // checkIdentifier에서 그 이름과 같은 식별자를 만나면 "자기 참조"로 판단한다.
    string previousDeclaring = currentlyDeclaring;
    currentlyDeclaring = decl->name;

    if (decl->initExpr != nullptr) {
        checkExpression(decl->initExpr);
    }

    currentlyDeclaring = previousDeclaring; // 중첩된 var 선언을 대비해 이전 상태로 복원

    // 초기화식 검사가 모두 끝난 뒤에야 비로소 변수를 현재 스코프에 등록한다.
    declare(decl->name);
}

void Checker::checkPrint(PrintStatement* stmt) {
    checkExpression(stmt->expr);
}

void Checker::checkIdentifier(IdentifierExpression* id) {
    // 지금 막 선언 중인 변수와 이름이 같다면 -> 자기 참조 에러
    if (!currentlyDeclaring.empty() && id->name == currentlyDeclaring) {
        reportError(id->getLine(), "자신의 초기화식에서 지역변수를 읽을 수 없습니다.");
        return;
    }

    // 그 외의 일반적인 경우: 스코프 체인 전체에서 선언 여부 확인.
    // (슬라이드에는 명시되어 있지 않지만, 미선언 변수 참조도 의미 오류이므로 함께 검사)
    if (!isDeclaredInAnyScope(id->name)) {
        reportError(id->getLine(), "'" + id->name + "'에러: 선언되지 않은 변수입니다.");
    }
}

void Checker::checkBinary(BinaryExpression* bin) {
    checkExpression(bin->left);
    checkExpression(bin->right);
}


CheckResult Checker::check(SyntaxTree& tree) {
    errors.clear();
    scopes.clear();
    currentlyDeclaring.clear();

    enterScope(); 

    // ASSUMPTION: SyntaxTree::getRoot()가 프로그램 전체를 감싸는 단일 Statement
    // (보통 최상위 BlockStatement)를 반환한다고 가정.
    checkStatement(dynamic_cast<Statement*>(tree.getRoot()));

    exitScope();

    CheckResult result;
    result.passed = errors.empty();
    result.errors = errors;
    return result;
}
