# CodeFab TODO 구현 가이드 (5인 분담)

> 이 문서는 `TODO.md`에 남은 작업을, 기존 `Implement.md`와 같은 형식(현재 상태 → 할 일 →
> 예시 코드 → 테스트 가이드)으로 **Tokenizer/Assembler/Checker/Executor/Shell 5명**이
> 병렬로 구현할 수 있도록 정리한다. 아래 §0의 팀 결정 사항을 전제로 작성했다.
>
> - 기존 모듈 담당자 배정을 유지한다.
> - **Shell 담당자는 Debug Mode 구현이 아직 끝나지 않아** 이번 라운드는 배정을 최소화했다.
> - **교차 모듈 계약(Token, SyntaxTree 노드, `*Interface.h`, 생성자 시그니처)을 또 바꿔야
>   하면 반드시 다른 담당자와 먼저 상의한다.**

## 0. 팀 결정 사항 및 미리 반영해 둔 인터페이스

이번에 다음 5가지가 결정됐다.

1. **`Super.field` 허용.** 별도 제한 없음 - 이유는 §2/§4에서 설명.
2. **클래스가 아닌 대상 상속은 Checker가 정적으로 검사한다.**
3. **import 대상 파일에 `Class` 선언을 허용한다.**
4. **Checker와 Optimizer(상수 폴딩) 책임을 분리한다.** 이번 라운드에 실제로 분리한다
   (설계만 하고 미루지 않는다).
5. **Visitor 패턴을 적용한다.** Executor부터 전환하고, Checker는 이번 범위 밖이다.

세 담당자(Assembler/Checker/Executor)가 이 결정 위에서 각자 코드를 짜다가 같은 타입을
살짝 다르게 선언하면 그 자체로 머지 충돌이 나므로, **공용 타입 계약만 미리 커밋**해뒀다
(파싱/의미검사/실행 로직과 테스트는 전혀 없음 - 아래 각 절에서 채운다).

| 파일 | 반영된 내용 |
|---|---|
| `Tokenizer/Token.h` | `TokenType`에 `SUPER`, `COLON` 추가 |
| `Assembler/SyntaxTree.h` | `SuperExpression` 노드(필드 없음, `ThisExpression`과 동일 모양) |
| `Assembler/SyntaxTree.h` | `ClassDeclareStatement`에 `IdentifierExpression* const superclass`(기본값 `nullptr`) 추가 - 기존 3-인자 생성 코드 그대로 컴파일됨 |
| `Assembler/SyntaxTree.h` | `SyntaxNodeVisitor` 인터페이스 추가, `SyntaxNode::accept(SyntaxNodeVisitor&)` 순수 가상 추가, **모든 구체(leaf) 노드에 `accept()` override 추가** (결정 5) |
| `Assembler/SyntaxTree.h` | `BinaryExpression::left/right`, `UnaryExpression::operand`를 `Expression* const` → `Expression*`로 완화(Architecture.md §6.2) - Optimizer가 폴딩 결과로 덮어쓸 수 있어야 하므로(결정 4) |
| `Checker/OptimizerInterface.h` | 신규 파일. `virtual void optimize(SyntaxTree&) = 0;` 하나뿐인 인터페이스(결정 4) |
| `CodeFab.vcxproj`/`.filters` | 위 신규 파일 등록 |

`Super.move(3)`은 별도 전용 노드로 만들지 않고, 기존 postfix 체인이
`FieldAccessExpression(object=SuperExpression, name="move")`/`CallExpression`으로 그대로
조립하도록 설계했다(This가 `IdentifierExpression`과 같은 경로를 타는 것과 동일한 재사용
원칙). **이 설계 덕분에 결정 1(`Super.field` 허용)은 Assembler/Checker 어느 쪽도 별도
분기를 추가할 필요가 없다** - postfix 체인은 이미 호출이든 필드 읽기든 구분 없이
`FieldAccessExpression`을 만들고, Executor만 그 `object`가 `SuperExpression`인지 보고
메서드 탐색 시작점을 옮기면 된다(§4).

## 1. Tokenizer 담당자

Tokenizer 자체의 신규 기능 TODO는 없다. 상속 토큰 스캔(다른 모든 상속 작업의 전제조건)과
모듈에 종속되지 않는 통합 테스트/문서 작업을 맡는다.

### 할 일 1: 상속 키워드/구분자 스캔

`Tokenizer.cpp`의 `KEYWORDS` 맵과 `scanToken()`에 한 줄씩만 추가하면 된다(§0에서 이미
`TokenType::SUPER`/`TokenType::COLON`이 추가되어 있음).

```cpp
static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    // ... 기존 항목들 ...
    { "Super", TokenType::SUPER },
};
```

```cpp
case ':': addToken(TokenType::COLON); break;
```

`TokenizerTest.cpp`에 `TokenizerTest, SuperKeyword_IsRecognized`, `TokenizerTest,
ColonSymbol_IsRecognized` 같은 이름으로 최소 1개씩 테스트를 추가한다(기존 `FuncKeyword_
IsRecognized` 패턴 그대로).

### 할 일 2: TODO.md #1 문서 갱신

`Implement.md` §5(Executor, "할 일 5: import 실행")의 의사코드가 여전히
`environment_.pushScope()` → 선언 실행 → `environment_.popScope();`(반환값 버림)로
되어 있는데, 실제 `Executor.cpp`의 `ImportStatement` 핸들러는 `ScopeGuard`로 임시
프레임을 연 채로 각 선언을 실행한 직후 `moduleScope->define(name, *environment_.lookup(name))`로
즉시 복사해 넣는 방식이다. 의사코드를 실제 구현에 맞게 고치고, `Architecture.md` §7.3에도
동일 내용을 반영한다.

### 할 일 3: TODO.md #2 문서 보강

Module은 Class와 달리 "필드/메서드 이중 컨테이너"가 필요 없다는 점을 명시한다: import로
뽑히는 선언은 `DeclareStatement`/`FunctionDeclareStatement`(+ 결정 3에 따라
`ClassDeclareStatement`)뿐이고, `FunctionDeclareStatement` 실행 핸들러가 함수도
`environment_.define(name, Value(decl))`로 "그냥 스코프의 값"으로 취급하므로 var든 Func든
`moduleScope` 하나에 나란히 들어간다 - `moduleScope->get(name)` 한 번으로 끝난다(Class처럼
"필드 → 없으면 메서드 목록"의 2단계 폴백이 필요 없음). `Architecture.md` §7.3 / `Implement.md`
§5에 반영.

### 할 일 4~6: Integration test 추가

`IntegrationTest/DebugIntegrationTest.cpp`(`RunPromptShellIntegrationTest` 스위트)에
TODO.md "# Integration test" 목록을 그대로 옮긴다.

- **§1 Function**: 함수 선언+호출+return, 재귀(`fact(5)` → `120`), return 없는 함수 →
  `nil`, 값 전달(파라미터 재대입이 호출자에 영향 없음), 함수 밖 return 에러, 파라미터
  이름 중복 에러, 인자 개수 불일치 에러, 함수 아닌 값 호출 에러.
- **§3 정적 배열**: 생성+인덱스 read/write, 변수 인덱스, 기본값(`nil`), 범위 밖 인덱스
  에러, 인덱스 타입 에러, 배열 아닌 값 인덱싱 에러, 크기 비-숫자 에러.
- **§5 instanceof**: `true`/`false` 케이스(상속 관련 instanceof는 §4에서 Executor
  담당자가 상속 완료 후 별도로 추가함).

## 2. Assembler 담당자

### 할 일 1: TODO.md #8 버그 수정

`Assembler.cpp:433-459`의 `makeBinaryExpression`에서 `default:`가 암묵적으로
`OrExpression`을 반환한다.

```cpp
// 지금 (버그)
case TokenType::AND: return addNode<AndExpression>(Tokens{ opToken }, left, right);
default: return addNode<OrExpression>(Tokens{ opToken }, left, right);
```

```cpp
// 수정
case TokenType::AND: return addNode<AndExpression>(Tokens{ opToken }, left, right);
case TokenType::OR: return addNode<OrExpression>(Tokens{ opToken }, left, right);
default: throw AssemblerError("makeBinaryExpression: 처리되지 않은 연산자 토큰입니다.");
```

우선순위 테이블에 새 이항 연산자를 추가하면서 이 `switch`에 `case`를 빠뜨리면 지금은
조용히 `OrExpression`이 되어버리는데, 위처럼 고치면 즉시 예외로 드러난다.

### 할 일 2: TODO.md #9 - 클래스 import 허용 (결정 3 반영)

`Assembler.cpp:284`의 허용 목록에 `ClassDeclareStatement`를 추가한다. Executor의
`declaredNameOf()`(`Executor.cpp:26-37`)는 이미 `ClassDeclareStatement` 분기가 있으므로
(지금까지는 죽은 코드였다) 이 변경만으로 살아난다 - Executor 담당자가 손댈 것 없음.

```cpp
// Assembler.cpp:284, 수정 전
if (!dynamic_cast<DeclareStatement*>(decl) && !dynamic_cast<FunctionDeclareStatement*>(decl)) {
    throw AssemblerError("import 대상 파일에는 선언 외의 내용을 허용하지 않습니다: '{}'", pathToken.origin);
}
```

```cpp
// 수정 후
if (!dynamic_cast<DeclareStatement*>(decl) && !dynamic_cast<FunctionDeclareStatement*>(decl)
    && !dynamic_cast<ClassDeclareStatement*>(decl)) {
    throw AssemblerError("import 대상 파일에는 선언 외의 내용을 허용하지 않습니다: '{}'", pathToken.origin);
}
```

`AssemblerImportTest`에 "`Class` 선언이 포함된 파일을 import해도 오류가 나지 않는다"는
회귀 테스트를 추가한다. (이 결정 덕분에 Executor 담당자는 TODO.md #13의 module 통합
테스트에서 더 이상 "Class는 나중에"로 미룰 필요 없이 처음부터 포함할 수 있다.)

### 할 일 3: 상속 문법 파싱

§0에서 추가된 `SUPER`/`COLON`/`SuperExpression`/`ClassDeclareStatement.superclass`를
채운다.

```cpp
// classDeclStmt -> CLASS IDENTIFIER (COLON IDENTIFIER)? LEFT_BRACE methodDeclStmt* RIGHT_BRACE
ClassDeclareStatement* parseClass() {
    Token classToken = popToken();
    Token name = popExpectedToken(TokenType::IDENTIFIER, "Expect class name.");

    IdentifierExpression* superclass = nullptr;
    if (auto t = currentToken(); t && t->type == TokenType::COLON) {
        popToken();
        Token superName = popExpectedToken(TokenType::IDENTIFIER, "Expect superclass name after ':'.");
        superclass = addNode<IdentifierExpression>(Tokens{ superName }, superName.origin);
    }

    Token leftBrace = popExpectedToken(TokenType::LEFT_BRACE, "Expect '{' before class body.");
    std::vector<MethodDeclareStatement*> methods;
    while (auto t = currentToken(); t && t->type != TokenType::RIGHT_BRACE) {
        methods.push_back(parseMethod());
    }
    Token rightBrace = popExpectedToken(TokenType::RIGHT_BRACE, "Expect '}' after class body.");
    return addNode<ClassDeclareStatement>(Tokens{ classToken, leftBrace, rightBrace }, name, methods, superclass);
}
```

`primary()`에 `SUPER` 분기를 `THIS`와 똑같이 추가한다 - `Super.move(3)`은 이 노드 하나만
만들면 기존 postfix 체인이 알아서 `FieldAccessExpression`/`CallExpression`으로 조립한다
(결정 1 덕분에 `Super.field`도 똑같이 조립되며 별도 분기가 필요 없다).

```cpp
case TokenType::SUPER:
    popToken();
    return addNode<SuperExpression>(Tokens{ *token });
```

`AssemblerTest.cpp`에 `Class B : A { }`(superclass 필드가 채워진 골든 트리),
`Super.move(3)`(`CallExpression(FieldAccessExpression(SuperExpression, "move"), [3])` 골든
트리) 파싱 테스트를 추가한다.

### 할 일 4: TODO.md #3

`import "sum.txt" alias sum; sum.func();`처럼 import 직후 `.` 접근을 한 입력 안에서
함께 검증하는 `AssemblerTest.cpp`(또는 `AssemblerImportTest`) 테스트를 추가한다 -
`[ImportStatement(...), ExpressionStatement(CallExpression(FieldAccessExpression(
IdentifierExpression("sum"), "func"), []))]`가 나오는지 확인.

## 3. Checker 담당자

### 할 일 1: TODO.md 코드 정리 #3 - `checkImport` 스코프 누락 수정 (먼저 처리)

`Checker.cpp:309-331`의 `checkImport`는 `enterScope()`/`exitScope()` 없이 현재 스코프에서
바로 `checkStatement(decl)`을 호출해서, import 내부 선언 이름이 바깥 스코프로 새어나간다
(PR #36 지적사항). `checkBlock`(`Checker.cpp:138-150`)과 동일한 패턴으로 고친다. **이
수정은 Executor 담당자의 `lookupAt`/`assignAt` 범위 검사 작업의 전제 조건**이므로 먼저
끝내고 공유한다.

```cpp
void Checker::checkImport(ImportStatement* importStmt) {
    if (forDepth > 0) {
        reportError(importStmt->getLine(), "반복문(for) 안에서는 import를 사용할 수 없습니다.");
    }
    const string& alias = importStmt->alias.origin;
    if (isDeclaredInCurrentScope(alias)) {
        reportError(importStmt->getLine(), "'" + alias + "'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다.");
    } else if (isDeclaredInAnyScope(alias)) {
        reportError(importStmt->getLine(), "'" + alias + "'에러: 상위 스코프에서 이미 사용중인 이름입니다.");
    }
    declare(alias);

    enterScope();
    try {
        for (Statement* decl : importStmt->declarations) {
            checkStatement(decl);
        }
    } catch (...) {
        exitScope();
        throw;
    }
    exitScope();
}
```

### 할 일 2: 상속 의미 검사 (결정 2 반영)

"자기 자신 상속 금지 / 클래스가 아닌 대상 상속 금지"를 Checker가 **정적으로** 잡기로
했으므로, 어떤 이름이 `Class`로 선언됐는지 추적하는 병행 스코프 스택이 필요하다(`scopes`가
이름 존재 여부만 추적하고 "무엇으로" 선언됐는지는 모르기 때문).

```cpp
// Checker.h에 추가
vector<unordered_set<string>> classNames_; // scopes와 나란히 push/pop, Class로 선언된 이름만
bool hasSuperclass_ = false;               // 0이 아니면(true) 현재 검사 중인 메서드가 상속 클래스 소속

void declareClass(const string& name);
bool isClassDeclaredInAnyScope(const string& name) const;
void checkSuper(SuperExpression* superExpr);
```

`enterScope()`/`exitScope()`가 `classNames_.emplace_back()`/`classNames_.pop_back()`도
함께 하도록 고친다(`scopes`와 항상 짝을 맞춰야 함).

```cpp
void Checker::declareClass(const string& name) {
    if (!classNames_.empty()) {
        classNames_.back().insert(name);
    }
}

bool Checker::isClassDeclaredInAnyScope(const string& name) const {
    for (auto it = classNames_.rbegin(); it != classNames_.rend(); ++it) {
        if (it->count(name) > 0) return true;
    }
    return false;
}
```

```cpp
void Checker::checkClass(ClassDeclareStatement* classDecl) {
    const string& name = classDecl->name.origin;
    if (isDeclaredInCurrentScope(name)) {
        reportError(classDecl->getLine(), "'" + name + "'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다.");
    }
    declare(name);
    declareClass(name);

    if (classDecl->superclass != nullptr) {
        const string& superName = classDecl->superclass->name;
        if (superName == name) {
            reportError(classDecl->getLine(), "'" + name + "' 클래스는 자기 자신을 상속할 수 없습니다.");
        }
        checkExpression(classDecl->superclass); // 존재 여부 검사 + depth 캐싱(resolveIdentifier)까지 재사용
        if (!isClassDeclaredInAnyScope(superName)) {
            reportError(classDecl->getLine(), "'" + superName + "'은(는) 클래스가 아니므로 상속할 수 없습니다.");
        }
    }

    bool previousHasSuper = hasSuperclass_;
    hasSuperclass_ = classDecl->superclass != nullptr;
    for (MethodDeclareStatement* method : classDecl->methods) {
        bool isInit = method->name.origin == "init";
        checkFunctionBody(method->name.origin, method->params, method->body, method->getLine(),
            /*isMethod=*/true, /*isInit=*/isInit);
    }
    hasSuperclass_ = previousHasSuper;
}
```

`checkExpression`에 `SuperExpression` 분기를 추가한다(지금은 분기가 없어 `Super`가
아무 검사 없이 그냥 통과한다).

```cpp
else if (auto* superExpr = dynamic_cast<SuperExpression*>(expr)) {
    checkSuper(superExpr);
}
```

```cpp
void Checker::checkSuper(SuperExpression* superExpr) {
    if (classMethodDepth == 0) {
        reportError(superExpr->getLine(), "클래스 메서드 밖에서 Super를 사용할 수 없습니다.");
    } else if (!hasSuperclass_) {
        reportError(superExpr->getLine(), "부모 클래스가 없는 클래스에서 Super를 사용할 수 없습니다.");
    }
}
```

`Super.field` 허용(결정 1)에 대해서는 **Checker가 별도로 할 일이 없다** - `FieldAccessExpression`
분기(`Checker.cpp:118-120`)가 이미 `object`를 재귀 검사할 뿐 종류를 가리지 않으므로,
`object`가 `SuperExpression`이든 `IdentifierExpression`이든 그대로 통과한다.

### 할 일 3: TODO.md #10 - Checker/Optimizer 분리 (결정 4, 실제 분리)

§0에서 `Checker/OptimizerInterface.h`(계약만)를 추가해뒀다. `Checker.cpp`의
`foldConstantIfPossible`(`Checker.cpp:227-239`)에 있던 ConstantFolder 로직을 그대로
떼어내 `Optimizer`로 옮긴다.

```cpp
// Checker/Optimizer.h (신규)
#pragma once
#include "OptimizerInterface.h"
#include "../Executor/ExecuteInterface.h"

class Optimizer : public OptimizerInterface {
public:
    explicit Optimizer(ExecuteInterface& executor) : executor_(executor) {}
    void optimize(SyntaxTree& tree) override;

private:
    ExecuteInterface& executor_;
    void foldStatement(Statement* stmt);
    Expression* foldExpression(Expression* expr); // 자식 자리에 새 리터럴을 꽂아 넣을 수 있도록 반환값으로 치환
};
```

```cpp
// Checker/Optimizer.cpp (신규) - Checker.cpp:227-239의 로직을 그대로 옮기되,
// §0에서 완화한 BinaryExpression::left/right(이제 const 아님)를 실제로 덮어써서
// "evaluate() 호출 확인"에서 "진짜 폴딩"으로 완성한다.
Expression* Optimizer::foldExpression(Expression* expr) {
    if (auto* bin = dynamic_cast<BinaryExpression*>(expr)) {
        bin->left = foldExpression(bin->left);
        bin->right = foldExpression(bin->right);
        if (isLiteral(bin->left) && isLiteral(bin->right)) {
            try {
                Value v = executor_.evaluate(bin);
                return replaceWithLiteral(v, bin); // tree.add()로 등록 후 반환
            } catch (const ExecutorError&) {
                // 0으로 나누기 등 - 원본을 그대로 둔다.
            }
        }
    }
    return expr;
}
```

`Checker`는 `foldConstantIfPossible` 호출부(`checkBinary`, `Checker.cpp:221-225`)와
그 구현을 제거하고, `check()`는 의미 오류 검사만 담당하도록 되돌린다. `Checker` 생성자
시그니처(`ExecuteInterface&`)는 그대로 유지한다(Resolver가 여전히 필요).

**Shell 담당자에게 공유할 내용**: `main.cpp`/`RunPromptShell`/`FileRunMode`가 이제
`checker_.check(tree)` 성공 직후, `executor_.execute(tree)` 이전에
`optimizer_.optimize(tree)`를 호출하도록 파이프라인에 한 단계를 끼워 넣어야 한다(§5 참고).

### 테스트 가이드

- `checkImport` 수정: 같은 스코프에서 import한 파일 안의 이름이 바깥 스코프에서 안 보이는지
  확인하는 테스트 추가.
- 상속 검사: `Class A : A` → 에러, `var x = 10; Class B : x { }` → 에러, 클래스 밖 `Super` →
  에러, 부모 없는 클래스의 메서드 안 `Super` → 에러.
- Optimizer 분리 후: `CheckerTest.cpp`는 의미 오류만 검증하도록 정리하고, 상수 폴딩
  테스트는 `OptimizerTest.cpp`(신규)로 옮긴다 - Fake `ExecuteInterface`로 `evaluate()`
  호출 횟수/폴딩 후 트리 구조를 검증하는 기존 테스트 그대로 재사용 가능.

## 4. Executor 담당자

### 할 일 1: 상속 실행 규칙

`findMethod` 헬퍼로 `instantiate`(`Executor.cpp:474-487`, `init` 탐색)와
`callMethod`(`Executor.cpp:489-526`, 임의 메서드 탐색)의 중복 선형 탐색을 통합하면서
(TODO.md 코드 정리 #4), 동시에 `superclass` 체인 탐색까지 구현한다(일석이조).

```cpp
MethodDeclareStatement* Executor::findMethod(const ClassDeclareStatement* klass, const std::string& name) {
    for (const ClassDeclareStatement* k = klass; k != nullptr; k = resolveSuperclass(k)) {
        for (MethodDeclareStatement* method : k->methods) {
            if (method->name.origin == name) {
                return method;
            }
        }
    }
    return nullptr;
}

const ClassDeclareStatement* Executor::resolveSuperclass(const ClassDeclareStatement* klass) {
    if (klass->superclass == nullptr) {
        return nullptr;
    }
    auto value = klass->superclass->depth
        ? environment_.lookupAt(*klass->superclass->depth, klass->superclass->name)
        : environment_.lookup(klass->superclass->name);
    if (!value || !value->isClass()) {
        throw ExecutorError("'{}'은(는) 클래스가 아닙니다.", klass->superclass->name);
    }
    return value->asClass();
}
```

`instantiate`/`callMethod`는 이제 `findMethod(klass, "init")`/`findMethod(instance->klass,
fieldAccess->name.origin)`을 호출하는 것으로 줄어든다.

**`Super.method(...)` 호출**: `callMethod`가 object를 평가하기 전에, `fieldAccess->object`가
`SuperExpression`인지 먼저 확인한다 - 맞으면 **현재 인스턴스(this)는 그대로 유지한 채**
메서드 탐색 시작점만 `superclass`로 옮긴다.

```cpp
Value Executor::callMethod(FieldAccessExpression* fieldAccess, const std::vector<Expression*>& argExprs) {
    if (dynamic_cast<SuperExpression*>(fieldAccess->object)) {
        auto thisValue = environment_.lookup("this");
        if (!thisValue || !thisValue->isInstance()) {
            throw ExecutorError("클래스 메서드 밖에서 Super를 사용했습니다.");
        }
        auto& instance = thisValue->asInstance();
        const ClassDeclareStatement* startClass = resolveSuperclass(instance->klass);
        MethodDeclareStatement* method = startClass ? findMethod(startClass, fieldAccess->name.origin) : nullptr;
        if (!method) {
            throw ExecutorError("'{}' 메서드가 부모 클래스에 존재하지 않습니다.", fieldAccess->name.origin);
        }
        return callMethodDecl(method, evaluateArgs(argExprs), *thisValue); // this는 원래 인스턴스 그대로
    }
    // ... 기존 object 평가 경로(모듈/인스턴스) ...
}
```

`Super.field`(결정 1)는 필드 저장소가 클래스 계층과 무관하게 인스턴스당 하나
(`instance->fields`)이므로, **`This.field`와 완전히 동일하게 동작**한다 - 별도 실행 경로가
필요 없다(`FieldAccessExpression` 값 읽기 핸들러가 `object`를 평가할 때 `SuperExpression`도
결국 `This`와 같은 인스턴스 값으로 평가되게만 해두면 된다 - `ThisExpression`과 동일하게
`"this"` 이름으로 동적 조회하는 핸들러를 `SuperExpression`에도 등록).

`InstanceOfExpression` 핸들러(`Executor.cpp:383-394`)를 포인터 비교 1회에서 체인 탐색으로
확장한다.

```cpp
expressionHandlers_[std::type_index(typeid(InstanceOfExpression))] = [this](Expression* expr) {
    auto* instOf = static_cast<InstanceOfExpression*>(expr);
    Value object = evaluate(instOf->object);
    if (!object.isInstance()) {
        return Value(false);
    }
    auto classValue = environment_.lookup(instOf->className.origin);
    if (!classValue || !classValue->isClass()) {
        throw ExecutorError("'{}'은(는) 클래스가 아닙니다.", instOf->className.origin);
    }
    for (const ClassDeclareStatement* k = object.asInstance()->klass; k != nullptr; k = resolveSuperclass(k)) {
        if (k == classValue->asClass()) {
            return Value(true);
        }
    }
    return Value(false);
};
```

### 할 일 2: TODO.md 코드 정리 #3 - `lookupAt`/`assignAt` 범위 검사

**Checker 담당자의 `checkImport` 스코프 수정(§3 할 일 1)이 먼저 반영됐는지 확인한 뒤**
작업한다(그 전에는 이 방어 코드가 진짜 스코프 불일치 버그를 가려버릴 수 있다).

```cpp
// Environment.cpp:42-50, 수정 전
std::optional<Value> Environment::lookupAt(int distance, const std::string& name) const {
    size_t index = scopes_.size() - 1 - static_cast<size_t>(distance);
    return scopes_[index].get(name);
}
```

```cpp
// 수정 후
std::optional<Value> Environment::lookupAt(int distance, const std::string& name) const {
    if (distance < 0 || static_cast<size_t>(distance) >= scopes_.size()) {
        throw ExecutorError("정적 바인딩 depth({})가 현재 스코프 개수({})를 벗어났습니다.", distance, scopes_.size());
    }
    size_t index = scopes_.size() - 1 - static_cast<size_t>(distance);
    return scopes_.at(index).get(name);
}
```

`assignAt`도 동일하게 고친다.

### 할 일 3: TODO.md 코드 정리 #5 - `evaluateArgs` 헬퍼

```cpp
std::vector<Value> Executor::evaluateArgs(const std::vector<Expression*>& argExprs) {
    std::vector<Value> args;
    args.reserve(argExprs.size());
    for (Expression* arg : argExprs) {
        args.push_back(evaluate(arg));
    }
    return args;
}
```

`Executor.cpp:311-314`(`CallExpression`), `498-502`(모듈 분기), `520-524`(인스턴스 분기)
세 곳을 `evaluateArgs(...)` 호출로 교체한다.

### 할 일 4: TODO.md #11 - Visitor 패턴 전환 (결정 5, Executor부터)

§0에서 `SyntaxTree.h`에 `SyntaxNodeVisitor`와 모든 leaf 노드의 `accept()`를 이미
추가해뒀다. `Executor`가 이 인터페이스를 구현하도록 전환한다.

```cpp
// Executor.h
class Executor : public ExecuteInterface, public SyntaxNodeVisitor {
public:
    // ...
    void execute(Statement* stmt) { stmt->accept(*this); }
    Value evaluate(Expression* expr) override {
        expr->accept(*this);
        return std::move(lastValue_);
    }

    // SyntaxNodeVisitor 구현 - Statement류는 side-effect만, Expression류는 lastValue_에 저장
    void visit(PrintStatement& stmt) override;
    void visit(IdentifierExpression& expr) override { lastValue_ = /* 기존 identifier 핸들러 로직 */; }
    // ... 나머지 전부 ...

private:
    Value lastValue_; // visit(Expression&)이 결과를 여기 담아두고 evaluate()가 꺼내 간다.
};
```

기존 `statementHandlers_`/`expressionHandlers_`(`std::unordered_map<std::type_index,
std::function<...>>`) 맵과 그 안의 람다들을 각 `visit(...)` 오버라이드로 옮긴다 - 람다
본문을 거의 그대로 메서드 바디로 옮기면 되므로 로직 자체는 바뀌지 않는다. `execute(Statement*)`/
`evaluate(Expression*)` 진입점만 `node->accept(*this)` 호출로 바뀐다.

**주의**: `SyntaxNodeVisitor`가 `void`만 반환하는 이유(값을 반환하지 않는 이유)는
`SyntaxTree.h`(Assembler 소유)가 `Executor/Value.h`에 의존하게 만들 수 없기 때문이다 -
반드시 `lastValue_` 같은 내부 상태를 거쳐야 한다. `Checker`는 이번 리팩토링 범위에서
제외한다(여전히 `dynamic_cast` 사용, `Checker.h:38-40` 주석에 이미 반영해둠).

### 할 일 5: TODO.md #12, #13 - Integration test

- **#12**: `Class Calc { fact(n) { if (n <= 1) return 1; return n * This.fact(n - 1); } }
  var c = Calc(); print c.fact(5);` → `120`. 재귀 중 `This`가 항상 같은 인스턴스를
  가리키는지 확인하는 케이스(필드 누적)도 추가.
- **#13**: 기본 module import(함수/변수 접근), 블록 스코프별 독립성, 여러 module 동시
  import 시 alias 충돌 없음 등. **결정 3(클래스 import 허용) 덕분에 `Class`가 섞인
  module도 처음부터 테스트에 포함할 수 있다** - 더 이상 단계적으로 미룰 필요 없음.

## 5. Shell 담당자 (Debug Mode 작업 중 - 배정 최소화)

Debug Mode(`DebugMode.h/.cpp`, `Debugger.h/.cpp`)가 진행 중이므로 새 기능은 배정하지
않는다. 대신 지금 하는 작업과 바로 이어지는 항목만 맡되, **Optimizer 분리(결정 4)로 인해
파이프라인에 한 단계가 늘어난 것**을 반영해야 한다.

### 할 일 1: TODO.md 코드 정리 #6 - `ShellRunner` 인터페이스

`main.cpp:46-62`의 `switch(args.mode)`가 Debug Mode 완성 시 계속 커질 구조이므로,
`RunPromptShell`/`FileRunMode`/(완성될) `DebugMode`가 공유할 실행 인터페이스를 만든다.

```cpp
// Shell/ShellRunner.h (신규)
#pragma once
class ShellRunner {
public:
    virtual ~ShellRunner() = default;
    virtual int run() = 0; // 성공 0, 실패 0이 아닌 값
};
```

`RunPromptShell::run(istream&, ostream&)`과 `FileRunMode::run(const string&, ostream&) ->
bool`처럼 시그니처가 서로 다른 문제는, 생성자에서 `istream&`/`ostream&`/`path` 등 필요한
입력을 전부 받아두고 `run()`은 인자 없이 호출하는 형태로 통일해서 해결한다(생성자 인자로
필요한 `*Interface&` 의존성은 그대로 주입받는다).

```cpp
main.cpp의 switch:
switch (args.mode) {
    case ShellMode::Repl: { RunPromptShell runner(tokenizer, assembler, checker, optimizer, executor, std::cin, std::cout); return runner.run(); }
    case ShellMode::Run:  { FileRunMode   runner(tokenizer, assembler, checker, optimizer, executor, args.path, std::cout); return runner.run(); }
    case ShellMode::Debug:{ DebugMode     runner(tokenizer, assembler, checker, optimizer, executor, args.path, std::cin, std::cout); return runner.run(); }
    default: return 1;
}
```

**Debug Mode 마무리보다 이 정리를 먼저 하는 것을 권장한다** - 나중에 하면 `DebugMode`도
다시 고쳐야 한다.

### 할 일 2: Optimizer를 파이프라인에 연결 (결정 4, Checker 담당자와 조율)

`main.cpp`에서 `Optimizer optimizer(executor);`를 만들어 `RunPromptShell`/`FileRunMode`/
`DebugMode` 생성자에 `OptimizerInterface&`로 주입한다. 각 러너 내부에서 파이프라인은:

```cpp
if (checker_.check(tree)) {
    optimizer_.optimize(tree);   // 신규: check 성공 직후, execute 이전
    executor_.execute(tree);
}
```

`optimize()`는 실패해도 예외를 던지지 않는 설계이므로(`OptimizerInterface.h` 참고) 별도
에러 처리가 필요 없다.

### 할 일 3: TODO.md "# Integration test" §7

Debug Mode가 완성되는 대로, 파일 모드 정상 실행/파일 없음/실행 중 오류 즉시 종료, 디버그
모드 breakpoint/step-next/watch/inspect 시나리오를 실제 4-Unit + Optimizer를 붙인 통합
테스트로 추가한다.

## 6. 공통 체크리스트

- [ ] `CodeFab.vcxproj`/`CodeFab.vcxproj.filters`에 새 파일(`Optimizer.h/.cpp`,
      `ShellRunner.h` 등)을 등록했는가.
- [ ] 기존 133개 테스트 + 새로 추가되는 테스트가 전부 통과하는가.
- [ ] 교차 모듈 계약을 또 바꿨다면 팀에 공유하고 이 문서를 갱신했는가.
- [ ] 에러 메시지(TODO.md 코드 정리 #1)와 주석(코드 정리 #2)은 각자 자기 담당 모듈
      파일을 손대는 김에 정리한다 - 다른 사람 파일까지 건드리면 충돌 위험이 커지므로
      별도 배정하지 않는다.
