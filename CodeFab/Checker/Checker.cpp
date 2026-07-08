#include "Checker.h"

// TODO(refactor): ConstantFolder는 아직 구현하지 못했다. BinaryExpression::left/right 등이
// 여전히 Expression* const라 폴딩 결과로 트리를 덮어쓸 방법이 없다. Assembler 쪽에서
// 해당 필드들의 constness를 완화해야 이어서 구현할 수 있다.

Checker::Checker(ExecuteInterface& executor) : executor_(executor) {
    enterScope(); // 세션 전체에 걸쳐 유지되는 전역 스코프
}

void Checker::enterScope() {
    scopes.emplace_back();
}

void Checker::exitScope() {
    scopes.pop_back();
}

bool Checker::isDeclaredInCurrentScope(const string& name) const {
    return !scopes.empty() && scopes.back().count(name) > 0;
}

bool Checker::isDeclaredInAnyScope(const string& name) const {
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
    throw CheckerError("[{}번째 줄] {}", line, message);
}

void Checker::checkStatement(Statement* stmt) {
    if (stmt == nullptr) {
        return;
    }

    if (auto* block = dynamic_cast<BlockStatement*>(stmt)) {
        checkBlock(block);
    }
    else if (auto* decl = dynamic_cast<DeclareStatement*>(stmt)) {
        checkDeclare(decl);
    }
    else if (auto* print = dynamic_cast<PrintStatement*>(stmt)) {
        checkPrint(print);
    }
    else if (auto* ifStmt = dynamic_cast<IfStatement*>(stmt)) {
        checkIf(ifStmt);
    }
    else if (auto* forStmt = dynamic_cast<ForStatement*>(stmt)) {
        checkFor(forStmt);
    }
    else if (auto* exprStmt = dynamic_cast<ExpressionStatement*>(stmt)) {
        checkExpression(exprStmt->expr);
    }
    else if (auto* funcDecl = dynamic_cast<FunctionDeclareStatement*>(stmt)) {
        const string& name = funcDecl->name.origin;
        if (isDeclaredInCurrentScope(name)) {
            reportError(funcDecl->getLine(), "'" + name + "'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다.");
        }
        // 바디 검사 전에 이름을 등록해 재귀 호출이 미선언 변수 오류로 잡히지 않게 한다.
        declare(name);
        checkFunctionBody(name, funcDecl->params, funcDecl->body, funcDecl->getLine(),
            /*isMethod=*/false, /*isInit=*/false);
    }
    else if (auto* classDecl = dynamic_cast<ClassDeclareStatement*>(stmt)) {
        checkClass(classDecl);
    }
    else if (auto* ret = dynamic_cast<ReturnStatement*>(stmt)) {
        checkReturn(ret);
    }
    else if (auto* importStmt = dynamic_cast<ImportStatement*>(stmt)) {
        checkImport(importStmt);
    }
    // MethodDeclareStatement는 클래스 바디 전용이라 여기 오지 않고 checkClass가 직접 처리한다.
}

void Checker::checkExpression(Expression* expr) {
    if (expr == nullptr) {
        return;
    }

    if (auto* id = dynamic_cast<IdentifierExpression*>(expr)) {
        checkIdentifier(id);
    }
    else if (auto* bin = dynamic_cast<BinaryExpression*>(expr)) {
        checkBinary(bin);
    }
    else if (auto* assign = dynamic_cast<AssignExpression*>(expr)) {
        checkExpression(assign->target);
        checkExpression(assign->value);
    }
    else if (auto* un = dynamic_cast<UnaryExpression*>(expr)) {
        checkExpression(un->operand);
    }
    else if (auto* call = dynamic_cast<CallExpression*>(expr)) {
        // 호출 대상/인자 개수 검증은 런타임 몫이라 여기선 재귀 검사만 한다.
        checkExpression(call->callee);
        for (Expression* arg : call->arguments) {
            checkExpression(arg);
        }
    }
    else if (auto* field = dynamic_cast<FieldAccessExpression*>(expr)) {
        checkExpression(field->object);
    }
    else if (auto* thisExpr = dynamic_cast<ThisExpression*>(expr)) {
        checkThis(thisExpr);
    }
    else if (auto* arr = dynamic_cast<ArrayExpression*>(expr)) {
        checkExpression(arr->sizeExpr);
    }
    else if (auto* idx = dynamic_cast<IndexExpression*>(expr)) {
        checkExpression(idx->collection);
        checkExpression(idx->index);
    }
    else if (auto* instOf = dynamic_cast<InstanceOfExpression*>(expr)) {
        // TODO(refactor): instOf->className은 Token이라 정적으로 선언 여부를 확인하지 않는다.
        checkExpression(instOf->object);
    }
    // 리터럴(Number/String/Boolean)은 항상 유효하므로 분기가 없다.
}

void Checker::checkBlock(BlockStatement* block) {
    enterScope();
    // scopes가 세션 내내 유지되므로 예외가 나도 exitScope는 반드시 실행돼야 한다.
    try {
        for (Statement* stmt : block->statements) {
            checkStatement(stmt);
        }
    } catch (...) {
        exitScope();
        throw;
    }
    exitScope();
}

void Checker::checkDeclare(DeclareStatement* decl) {
    const string& name = decl->identifier->name;

    if (isDeclaredInCurrentScope(name)) {
        reportError(decl->getLine(),
            "'" + name + "'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.");
    }

    // 초기화식 검사 중에는 아직 이름을 등록하지 않고, currentlyDeclaring에 기억해두어
    // checkIdentifier가 자기 참조(var a = a + 1;)를 잡을 수 있게 한다.
    string previousDeclaring = currentlyDeclaring;
    currentlyDeclaring = name;

    checkExpression(decl->expr);

    currentlyDeclaring = previousDeclaring;

    declare(name);
}

void Checker::checkPrint(PrintStatement* stmt) {
    checkExpression(stmt->expr);
}

void Checker::checkIf(IfStatement* ifStmt) {
    checkExpression(ifStmt->expr);
    checkStatement(ifStmt->thenBranch);
    checkStatement(ifStmt->elseBranch);
}

void Checker::checkFor(ForStatement* forStmt) {
    // init에서 선언한 변수가 루프 본문에서만 보여야 하므로 전용 스코프를 연다.
    enterScope();
    ++forDepth;
    try {
        checkStatement(forStmt->init);
        checkExpression(forStmt->compare);
        checkExpression(forStmt->next);
        checkStatement(forStmt->loop);
    } catch (...) {
        --forDepth;
        exitScope();
        throw;
    }
    --forDepth;
    exitScope();
}

void Checker::checkIdentifier(IdentifierExpression* id) {
    if (!currentlyDeclaring.empty() && id->name == currentlyDeclaring) {
        reportError(id->getLine(), "자신의 초기화식에서 지역변수를 읽을 수 없습니다.");
    }

    if (!isDeclaredInAnyScope(id->name)) {
        reportError(id->getLine(), "'" + id->name + "'에러: 선언되지 않은 변수입니다.");
    }

    resolveIdentifier(id);
}

void Checker::checkBinary(BinaryExpression* bin) {
    checkExpression(bin->left);
    checkExpression(bin->right);
}

void Checker::checkFunctionBody(const string& name, const vector<Token>& params,
    const vector<Statement*>& body, int line, bool isMethod, bool isInit) {
    unordered_set<string> paramNames;
    for (const Token& param : params) {
        if (!paramNames.insert(param.origin).second) {
            reportError(line, "'" + name + "'의 파라미터 이름 '" + param.origin + "'이(가) 중복됩니다.");
        }
    }

    enterScope();
    ++functionDepth;
    if (isMethod) {
        ++classMethodDepth;
        declare("this");
    }
    bool previousInInit = inInitMethod;
    inInitMethod = isInit;

    try {
        for (const Token& param : params) {
            declare(param.origin);
        }
        for (Statement* stmt : body) {
            checkStatement(stmt);
        }
    } catch (...) {
        inInitMethod = previousInInit;
        if (isMethod) {
            --classMethodDepth;
        }
        --functionDepth;
        exitScope();
        throw;
    }

    inInitMethod = previousInInit;
    if (isMethod) {
        --classMethodDepth;
    }
    --functionDepth;
    exitScope();
}

void Checker::checkClass(ClassDeclareStatement* classDecl) {
    const string& name = classDecl->name.origin;
    if (isDeclaredInCurrentScope(name)) {
        reportError(classDecl->getLine(), "'" + name + "'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다.");
    }
    declare(name);

    // TODO(refactor): 같은 클래스 안에서 메서드 이름이 중복돼도 지금은 검사하지 않는다.
    for (MethodDeclareStatement* method : classDecl->methods) {
        bool isInit = method->name.origin == "init";
        checkFunctionBody(method->name.origin, method->params, method->body, method->getLine(),
            /*isMethod=*/true, /*isInit=*/isInit);
    }
}

void Checker::checkReturn(ReturnStatement* ret) {
    if (functionDepth == 0) {
        reportError(ret->getLine(), "함수(메서드) 밖에서 return을 사용할 수 없습니다.");
    }
    if (inInitMethod && ret->value != nullptr) {
        reportError(ret->getLine(), "init 메서드는 값을 반환할 수 없습니다.");
    }
    checkExpression(ret->value);
}

void Checker::checkImport(ImportStatement* importStmt) {
    if (forDepth > 0) {
        reportError(importStmt->getLine(), "반복문(for) 안에서는 import를 사용할 수 없습니다.");
    }

    const string& alias = importStmt->alias.origin;

    // TODO(refactor): ImportStatement에 원본 path가 없어 "동일 alias"로 대체 검사 중이다.
    // 서로 다른 alias로 같은 파일을 두 번 import하는 경우는 걸러내지 못한다.
    if (isDeclaredInCurrentScope(alias)) {
        reportError(importStmt->getLine(), "'" + alias + "'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다.");
    }
    else if (isDeclaredInAnyScope(alias)) {
        reportError(importStmt->getLine(), "'" + alias + "'에러: 상위 스코프에서 이미 사용중인 이름입니다.");
    }

    declare(alias);

    // 파일 존재/순환 검사는 Assembler가 이미 끝냈으므로 declarations는 그대로 재귀 검사만 한다.
    for (Statement* decl : importStmt->declarations) {
        checkStatement(decl);
    }
}

void Checker::checkThis(ThisExpression* thisExpr) {
    if (classMethodDepth == 0) {
        reportError(thisExpr->getLine(), "클래스 메서드 밖에서 This를 사용할 수 없습니다.");
    }
    // this는 checkFunctionBody가 메서드 스코프에 미리 declare해두므로 여기선 추가 처리가 없다.
}

void Checker::resolveIdentifier(IdentifierExpression* id) const {
    for (int distance = 0; distance < static_cast<int>(scopes.size()); ++distance) {
        const auto& scope = scopes[scopes.size() - 1 - distance];
        if (scope.count(id->name) > 0) {
            id->depth = distance;
            return;
        }
    }
    // checkIdentifier가 먼저 isDeclaredInAnyScope로 확인하므로 이론상 도달하지 않는다.
    id->depth = std::nullopt;
}

bool Checker::check(SyntaxTree& tree) {
    // scopes를 제외한 나머지는 이번 호출(REPL 한 줄) 한정 상태라 매번 초기화한다.
    currentlyDeclaring.clear();
    functionDepth = 0;
    classMethodDepth = 0;
    forDepth = 0;
    inInitMethod = false;

    checkStatement(dynamic_cast<Statement*>(tree.getRoot()));

    return true;
}
