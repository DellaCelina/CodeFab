#include "Checker.h"

namespace {

bool isLiteralExpression(Expression* expr) {
    return dynamic_cast<NumberExpression*>(expr) != nullptr
        || dynamic_cast<BooleanExpression*>(expr) != nullptr
        || dynamic_cast<StringExpression*>(expr) != nullptr;
}

}  // namespace

Checker::Checker(ExecuteInterface& executor) : executor_(executor) {
    enterScope(); // 세션 전체에 걸쳐 유지되는 전역 스코프
    registerDefaultHandlers();
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

void Checker::registerDefaultHandlers() {
    statementHandlers_[type_index(typeid(BlockStatement))] = [this](Statement* stmt) {
        checkBlock(static_cast<BlockStatement*>(stmt));
    };
    statementHandlers_[type_index(typeid(DeclareStatement))] = [this](Statement* stmt) {
        checkDeclare(static_cast<DeclareStatement*>(stmt));
    };
    statementHandlers_[type_index(typeid(PrintStatement))] = [this](Statement* stmt) {
        checkPrint(static_cast<PrintStatement*>(stmt));
    };
    statementHandlers_[type_index(typeid(IfStatement))] = [this](Statement* stmt) {
        checkIf(static_cast<IfStatement*>(stmt));
    };
    statementHandlers_[type_index(typeid(ForStatement))] = [this](Statement* stmt) {
        checkFor(static_cast<ForStatement*>(stmt));
    };
    statementHandlers_[type_index(typeid(ExpressionStatement))] = [this](Statement* stmt) {
        checkExpression(static_cast<ExpressionStatement*>(stmt)->expr);
    };
    statementHandlers_[type_index(typeid(FunctionDeclareStatement))] = [this](Statement* stmt) {
        checkFunctionDeclare(static_cast<FunctionDeclareStatement*>(stmt));
    };
    statementHandlers_[type_index(typeid(ClassDeclareStatement))] = [this](Statement* stmt) {
        checkClass(static_cast<ClassDeclareStatement*>(stmt));
    };
    statementHandlers_[type_index(typeid(ReturnStatement))] = [this](Statement* stmt) {
        checkReturn(static_cast<ReturnStatement*>(stmt));
    };
    statementHandlers_[type_index(typeid(ImportStatement))] = [this](Statement* stmt) {
        checkImport(static_cast<ImportStatement*>(stmt));
    };
    // MethodDeclareStatement는 클래스 바디 전용이라 핸들러가 없다 - checkClass가 직접 처리한다.

    expressionHandlers_[type_index(typeid(IdentifierExpression))] = [this](Expression* expr) {
        checkIdentifier(static_cast<IdentifierExpression*>(expr));
    };
    expressionHandlers_[type_index(typeid(AssignExpression))] = [this](Expression* expr) {
        auto* assign = static_cast<AssignExpression*>(expr);
        checkExpression(assign->target);
        checkExpression(assign->value);
    };
    expressionHandlers_[type_index(typeid(CallExpression))] = [this](Expression* expr) {
        // 호출 대상/인자 개수 검증은 런타임 몫이라 여기선 재귀 검사만 한다.
        auto* call = static_cast<CallExpression*>(expr);
        checkExpression(call->callee);
        for (Expression* arg : call->arguments) {
            checkExpression(arg);
        }
    };
    expressionHandlers_[type_index(typeid(FieldAccessExpression))] = [this](Expression* expr) {
        checkExpression(static_cast<FieldAccessExpression*>(expr)->object);
    };
    expressionHandlers_[type_index(typeid(ThisExpression))] = [this](Expression* expr) {
        checkThis(static_cast<ThisExpression*>(expr));
    };
    expressionHandlers_[type_index(typeid(ArrayExpression))] = [this](Expression* expr) {
        checkExpression(static_cast<ArrayExpression*>(expr)->sizeExpr);
    };
    expressionHandlers_[type_index(typeid(IndexExpression))] = [this](Expression* expr) {
        auto* idx = static_cast<IndexExpression*>(expr);
        checkExpression(idx->collection);
        checkExpression(idx->index);
    };
    expressionHandlers_[type_index(typeid(InstanceOfExpression))] = [this](Expression* expr) {
        // TODO(refactor): instOf->className은 Token이라 정적으로 선언 여부를 확인하지 않는다.
        checkExpression(static_cast<InstanceOfExpression*>(expr)->object);
    };

    // type_index는 정확한 타입만 매칭되므로 BinaryExpression 하위 타입마다 등록해야 한다.
    // 새 이항 연산자가 추가되면 여기 한 줄 추가.
    auto binaryHandler = [this](Expression* expr) {
        checkBinary(static_cast<BinaryExpression*>(expr));
    };
    expressionHandlers_[type_index(typeid(AddExpression))] = binaryHandler;
    expressionHandlers_[type_index(typeid(SubExpression))] = binaryHandler;
    expressionHandlers_[type_index(typeid(MultExpression))] = binaryHandler;
    expressionHandlers_[type_index(typeid(DivideExpression))] = binaryHandler;
    expressionHandlers_[type_index(typeid(EqualExpression))] = binaryHandler;
    expressionHandlers_[type_index(typeid(NotEqualExpression))] = binaryHandler;
    expressionHandlers_[type_index(typeid(LessExpression))] = binaryHandler;
    expressionHandlers_[type_index(typeid(LessEqualExpression))] = binaryHandler;
    expressionHandlers_[type_index(typeid(GreaterExpression))] = binaryHandler;
    expressionHandlers_[type_index(typeid(GreaterEqualExpression))] = binaryHandler;

    auto unaryHandler = [this](Expression* expr) {
        checkExpression(static_cast<UnaryExpression*>(expr)->operand);
    };
    expressionHandlers_[type_index(typeid(NegativeExpression))] = unaryHandler;
    expressionHandlers_[type_index(typeid(NotExpression))] = unaryHandler;

    // 리터럴(Number/String/Boolean)은 항상 유효하므로 핸들러가 없다.
}

void Checker::checkStatement(Statement* stmt) {
    if (stmt == nullptr) {
        return;
    }
    auto it = statementHandlers_.find(type_index(typeid(*stmt)));
    if (it != statementHandlers_.end()) {
        it->second(stmt);
    }
}

void Checker::checkExpression(Expression* expr) {
    if (expr == nullptr) {
        return;
    }
    auto it = expressionHandlers_.find(type_index(typeid(*expr)));
    if (it != expressionHandlers_.end()) {
        it->second(expr);
    }
}

void Checker::checkFunctionDeclare(FunctionDeclareStatement* funcDecl) {
    const string& name = funcDecl->name.origin;
    if (isDeclaredInCurrentScope(name)) {
        reportError(funcDecl->getLine(), "'" + name + "'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다.");
    }
    // 바디 검사 전에 이름을 등록해 재귀 호출이 미선언 변수 오류로 잡히지 않게 한다.
    declare(name);
    checkFunctionBody(name, funcDecl->params, funcDecl->body, funcDecl->getLine(),
        /*isMethod=*/false, /*isInit=*/false);
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
    foldConstantIfPossible(bin);
}

void Checker::foldConstantIfPossible(BinaryExpression* bin) {
    if (!isLiteralExpression(bin->left) || !isLiteralExpression(bin->right)) {
        return;
    }
    try {
        executor_.evaluate(bin);
        // TODO(refactor): 여기서 얻은 값으로 bin 자리를 리터럴 노드로 치환해야 진짜 폴딩이다.
        // BinaryExpression::left/right가 Expression* const라 지금은 트리를 바꾸지 못하고,
        // evaluate()가 호출된다는 것만 확인한다.
    } catch (const ExecutorError&) {
        // 0으로 나누기 등 - 컴파일 타임에 대신 오류를 내면 안 되므로 조용히 건너뛴다.
    }
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
