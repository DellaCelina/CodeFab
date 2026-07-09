#include "Checker.h"

Checker::Checker() {
    enterScope(); // 세션 전체에 걸쳐 유지되는 전역 스코프
}

void Checker::enterScope() {
    scopes.emplace_back();
    classNames_.emplace_back();
}

void Checker::exitScope() {
    scopes.pop_back();
    classNames_.pop_back();
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

void Checker::declareClass(const string& name) {
    if (!classNames_.empty()) {
        classNames_.back().insert(name);
    }
}

bool Checker::isClassDeclaredInAnyScope(const string& name) const {
    for (auto it = classNames_.rbegin(); it != classNames_.rend(); ++it) {
        if (it->count(name) > 0) {
            return true;
        }
    }
    return false;
}

void Checker::reportError(int line, const string& message) {
    throw CheckerError("[{}번째 줄] {}", line, message);
}

void Checker::checkStatement(Statement* stmt) {
    stmt->accept(*this);
}

void Checker::checkExpression(Expression* expr) {
    expr->accept(*this);
}

void Checker::checkBinary(BinaryExpression& bin) {
    checkExpression(bin.left);
    checkExpression(bin.right);
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

void Checker::resolveIdentifier(IdentifierExpression& id) const {
    for (int distance = 0; distance < static_cast<int>(scopes.size()); ++distance) {
        const auto& scope = scopes[scopes.size() - 1 - distance];
        if (scope.count(id.name) > 0) {
            id.depth = distance;
            return;
        }
    }
    // checkIdentifier가 먼저 isDeclaredInAnyScope로 확인하므로 이론상 도달하지 않는다.
    id.depth = std::nullopt;
}

void Checker::visit(IdentifierExpression& node) {
    if (!currentlyDeclaring.empty() && node.name == currentlyDeclaring) {
        reportError(node.getLine(), "자신의 초기화식에서 지역변수를 읽을 수 없습니다.");
    }

    if (!isDeclaredInAnyScope(node.name)) {
        reportError(node.getLine(), "'" + node.name + "'에러: 선언되지 않은 변수입니다.");
    }

    // 함수/메서드 본문 안에서는 정적 바인딩을 건너뛴다: Environment는 함수 호출마다
    // (재귀 호출 포함) 스코프를 하나씩 더 쌓는 콜스택 구조라, 여기서 계산하는 "몇 단계
    // 위 스코프인지"는 선언 시점 기준일 뿐 재귀 호출 깊이에 따라 실제 런타임 스코프
    // 깊이와 달라진다 - 예: fact가 자기 자신을 재귀 호출하면 재귀가 한 단계 깊어질
    // 때마다 실제로는 몇 단계를 더 올라가야 fact를 찾는데 depth는 1로 고정돼 있어
    // 엉뚱한 스코프를 가리키게 된다. 함수/메서드 밖(블록/전역)은 재귀 호출 없이
    // 코드 구조 그대로 스코프가 쌓이므로 정적 바인딩이 여전히 안전하다.
    if (functionDepth == 0) {
        resolveIdentifier(node);
    }
}

void Checker::visit(PrintStatement& node) {
    checkExpression(node.expr);
}

void Checker::visit(ExpressionStatement& node) {
    checkExpression(node.expr);
}

void Checker::visit(DeclareStatement& node) {
    const string& name = node.identifier->name;

    if (isDeclaredInCurrentScope(name)) {
        reportError(node.getLine(),
            "'" + name + "'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.");
    }

    // 초기화식 검사 중에는 아직 이름을 등록하지 않고, currentlyDeclaring에 기억해두어
    // checkIdentifier가 자기 참조(var a = a + 1;)를 잡을 수 있게 한다.
    string previousDeclaring = currentlyDeclaring;
    currentlyDeclaring = name;

    checkExpression(node.expr);

    currentlyDeclaring = previousDeclaring;

    declare(name);
}

void Checker::visit(BlockStatement& node) {
    enterScope();
    // scopes가 세션 내내 유지되므로 예외가 나도 exitScope는 반드시 실행돼야 한다.
    try {
        for (Statement* stmt : node.statements) {
            checkStatement(stmt);
        }
    } catch (...) {
        exitScope();
        throw;
    }
    exitScope();
}

void Checker::visit(IfStatement& node) {
    checkExpression(node.expr);
    checkStatement(node.thenBranch);
    if (node.elseBranch) {
        checkStatement(node.elseBranch);
    }
}

void Checker::visit(ForStatement& node) {
    // init에서 선언한 변수가 루프 본문에서만 보여야 하므로 전용 스코프를 연다.
    enterScope();
    ++forDepth;
    try {
        checkStatement(node.init);
        checkExpression(node.compare);
        checkExpression(node.next);
        checkStatement(node.loop);
    } catch (...) {
        --forDepth;
        exitScope();
        throw;
    }
    --forDepth;
    exitScope();
}

// 리터럴은 항상 유효하므로 검사할 게 없다.
void Checker::visit(NumberExpression&) {}
void Checker::visit(StringExpression&) {}
void Checker::visit(BooleanExpression&) {}

void Checker::visit(AddExpression& node) { checkBinary(node); }
void Checker::visit(MultExpression& node) { checkBinary(node); }
void Checker::visit(SubExpression& node) { checkBinary(node); }
void Checker::visit(DivideExpression& node) { checkBinary(node); }
void Checker::visit(ModExpression& node) { checkBinary(node); }
void Checker::visit(AndExpression& node) { checkBinary(node); }
void Checker::visit(OrExpression& node) { checkBinary(node); }
void Checker::visit(EqualExpression& node) { checkBinary(node); }
void Checker::visit(NotEqualExpression& node) { checkBinary(node); }
void Checker::visit(LessExpression& node) { checkBinary(node); }
void Checker::visit(LessEqualExpression& node) { checkBinary(node); }
void Checker::visit(GreaterExpression& node) { checkBinary(node); }
void Checker::visit(GreaterEqualExpression& node) { checkBinary(node); }

void Checker::visit(AssignExpression& node) {
    checkExpression(node.target);
    checkExpression(node.value);
}

void Checker::visit(NegativeExpression& node) {
    checkExpression(node.operand);
}

void Checker::visit(NotExpression& node) {
    checkExpression(node.operand);
}

void Checker::visit(CallExpression& node) {
    // 호출 대상/인자 개수 검증은 런타임 몫이라 여기선 재귀 검사만 한다.
    checkExpression(node.callee);
    for (Expression* arg : node.arguments) {
        checkExpression(arg);
    }
}

void Checker::visit(FieldAccessExpression& node) {
    checkExpression(node.object);
}

void Checker::visit(ThisExpression& node) {
    if (classMethodDepth == 0) {
        reportError(node.getLine(), "클래스 메서드 밖에서 This를 사용할 수 없습니다.");
    }
    // this는 checkFunctionBody가 메서드 스코프에 미리 declare해두므로 여기선 추가 처리가 없다.
}

void Checker::visit(SuperExpression& node) {
    if (classMethodDepth == 0) {
        reportError(node.getLine(), "클래스 메서드 밖에서 Super를 사용할 수 없습니다.");
    } else if (!hasSuperclass_) {
        reportError(node.getLine(), "부모 클래스가 없는 클래스에서 Super를 사용할 수 없습니다.");
    }
}

void Checker::visit(ArrayExpression& node) {
    checkExpression(node.sizeExpr);
}

void Checker::visit(IndexExpression& node) {
    checkExpression(node.collection);
    checkExpression(node.index);
}

void Checker::visit(InstanceOfExpression& node) {
    // TODO(refactor): node.className은 Token이라 정적으로 선언 여부를 확인하지 않는다.
    checkExpression(node.object);
}

void Checker::visit(FunctionDeclareStatement& node) {
    const string& name = node.name.origin;
    if (isDeclaredInCurrentScope(name)) {
        reportError(node.getLine(), "'" + name + "'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다.");
    }
    // 바디 검사 전에 이름을 등록해 재귀 호출이 미선언 변수 오류로 잡히지 않게 한다.
    declare(name);
    checkFunctionBody(name, node.params, node.body, node.getLine(),
        /*isMethod=*/false, /*isInit=*/false);
}

void Checker::visit(MethodDeclareStatement&) {
    // 클래스 바디 전용 선언이라 accept()로 직접 방문되지 않는다 -
    // visit(ClassDeclareStatement&)가 checkFunctionBody로 바로 처리한다.
}

void Checker::visit(ReturnStatement& node) {
    if (functionDepth == 0) {
        reportError(node.getLine(), "함수(메서드) 밖에서 return을 사용할 수 없습니다.");
    }
    if (inInitMethod && node.value != nullptr) {
        reportError(node.getLine(), "init 메서드는 값을 반환할 수 없습니다.");
    }
    if (node.value) {
        checkExpression(node.value);
    }
}

void Checker::visit(ClassDeclareStatement& node) {
    const string& name = node.name.origin;
    if (isDeclaredInCurrentScope(name)) {
        reportError(node.getLine(), "'" + name + "'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다.");
    }
    declare(name);
    declareClass(name);

    if (node.superclass != nullptr) {
        const string& superName = node.superclass->name;
        if (superName == name) {
            reportError(node.getLine(), "'" + name + "' 클래스는 자기 자신을 상속할 수 없습니다.");
        }
        checkExpression(node.superclass); // 존재 여부 검사 + depth 캐싱(resolveIdentifier)까지 재사용
        if (!isClassDeclaredInAnyScope(superName)) {
            reportError(node.getLine(), "'" + superName + "'은(는) 클래스가 아니므로 상속할 수 없습니다.");
        }
    }

    // TODO(refactor): 같은 클래스 안에서 메서드 이름이 중복돼도 지금은 검사하지 않는다.
    bool previousHasSuper = hasSuperclass_;
    hasSuperclass_ = node.superclass != nullptr;
    for (MethodDeclareStatement* method : node.methods) {
        bool isInit = method->name.origin == "init";
        checkFunctionBody(method->name.origin, method->params, method->body, method->getLine(),
            /*isMethod=*/true, /*isInit=*/isInit);
    }
    hasSuperclass_ = previousHasSuper;
}

void Checker::visit(ImportStatement& node) {
    if (forDepth > 0) {
        reportError(node.getLine(), "반복문(for) 안에서는 import를 사용할 수 없습니다.");
    }

    const string& alias = node.alias.origin;

    // TODO(refactor): ImportStatement에 원본 path가 없어 "동일 alias"로 대체 검사 중이다.
    // 서로 다른 alias로 같은 파일을 두 번 import하는 경우는 걸러내지 못한다.
    if (isDeclaredInCurrentScope(alias)) {
        reportError(node.getLine(), "'" + alias + "'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다.");
    }
    else if (isDeclaredInAnyScope(alias)) {
        reportError(node.getLine(), "'" + alias + "'에러: 상위 스코프에서 이미 사용중인 이름입니다.");
    }

    declare(alias);

    // import 내부 선언은 전용 스코프에서 검사한다 - 그러지 않으면 이름이 바깥 스코프로
    // 새어나간다(PR #36 지적사항). 파일 존재/순환 검사는 Assembler가 이미 끝냈으므로
    // declarations는 재귀 검사만 한다.
    enterScope();
    try {
        for (Statement* decl : node.declarations) {
            checkStatement(decl);
        }
    } catch (...) {
        exitScope();
        throw;
    }
    exitScope();
}

void Checker::check(SyntaxTree& tree) {
    // scopes를 제외한 나머지는 이번 호출(REPL 한 줄) 한정 상태라 매번 초기화한다.
    currentlyDeclaring.clear();
    functionDepth = 0;
    classMethodDepth = 0;
    forDepth = 0;
    inInitMethod = false;

    auto* root = dynamic_cast<Statement*>(tree.getRoot());
    if (!root) {
        throw std::logic_error("Checker::check: tree root is not a Statement");
    }
    checkStatement(root);
}
