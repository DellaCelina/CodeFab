# TODO

이 문서는 설계/구현 논의 중 발견된, **아직 Architecture.md/Implement.md/코드 어디에도
확정된 해법이 없는 빈틈**을 추적한다. 실제로 손을 대기 전에 여기 목록을 먼저 확인한다.

## 1. Executor: Import 실행 시 moduleScope가 채워지지 않는 문제

**증상**: Implement.md §5(Executor 담당자 가이드, "할 일 5: import 실행")의 현재
의사코드는 아래처럼 되어 있는데, 실제로는 동작하지 않는다.

```cpp
auto moduleScope = std::make_shared<Scope>();
environment_.pushScope();                 // 임시 스코프 하나 push
for (Statement* decl : importStmt->declarations) execute(decl);
// -> 방금 실행된 var/Func 선언들은 "임시 스코프"에 등록된다
environment_.popScope();                  // 임시 스코프를 그냥 버림!
environment_.define(importStmt->alias.origin, Value(moduleScope)); // moduleScope는 여전히 빈 상태
```

`Environment`가 `std::vector<Scope> scopes_`로 **값(value) 저장**하고 있고
`Scope`에는 내용을 꺼내거나 옮길 수 있는 API(순회/이동)가 없어서, 임시 스코프에
실행된 선언들을 `moduleScope`로 옮길 방법이 코드로 확정되어 있지 않다. Implement.md도
"Scope에 내용을 훑는 API가 없다면 필요한 만큼만 추가한다"라고만 적어두고 구체적인
해법은 없는 상태 — 즉 **알려진 미해결 갭**이다.

**결과적으로**: `import "sum.txt" alias sum;` 이후 `sum.func()`나 `sum.value`에
접근하면(Executor의 FieldAccessExpression/CallExpression 핸들러 자체도 아직
없지만, 그걸 구현하더라도) alias `sum`이 가리키는 `moduleScope`가 항상 비어있어서
"필드/함수가 존재하지 않습니다" 오류가 난다.

**제안하는 해법**: `Environment::popScope()`의 반환 타입을 `void`에서 제거되는
`Scope`를 반환하도록 바꾼다.

```cpp
// Environment.h
Scope popScope(); // 기존: void popScope();
```

```cpp
// Executor.cpp, ImportStatement 핸들러
environment_.pushScope();
for (Statement* decl : importStmt->declarations) {
    execute(decl);
}
Scope executed = environment_.popScope();
*moduleScope = std::move(executed);
environment_.define(importStmt->alias.origin, Value(moduleScope));
```

`Environment`는 Checker(정적 바인딩)와 Executor가 공유하는 인터페이스이므로, 이
시그니처를 바꾸면 두 모듈 담당자에게 공유해야 한다(현재 `popScope()`를 호출하는
다른 곳 - `ScopeGuard` 소멸자, `ForStatement`/`BlockStatement` 핸들러 - 는 반환값을
버리기만 하면 되므로 하위 호환에는 문제없음).

**해야 할 일**:
- [ ] `Environment::popScope()` 시그니처를 `Scope popScope()`로 변경 (Environment.h/.cpp)
- [ ] `Scope`가 이동 가능한지 확인(현재 `unordered_map` 멤버 하나뿐이라 기본 이동
      생성자로 충분할 것)
- [ ] Executor의 `ImportStatement` 핸들러를 위 예시처럼 구현
- [ ] `FieldAccessExpression`/`CallExpression`이 `object.isModule()`일 때
      `object.asModule()->get(name)`으로 조회하는 분기 추가
- [ ] Architecture.md §7.3, Implement.md §5(Executor)를 위 해법으로 갱신

## 2. Module은 Class와 다르게 "field/method 이중 컨테이너"가 필요 없다는 점을 문서에 명확히

현재 Architecture.md §7.3 / Implement.md §5 문구("§4.3의 필드/메서드 조회와 사실상
같은 코드 경로")가 Class처럼 "필드 → 없으면 메서드 목록"의 2단계 폴백이 Module에도
필요한 것처럼 오해될 여지가 있다. 실제로는:

- Class: `instance->fields`(Scope, var만) + `klass->methods`(별도 목록, `this` 바인딩이
  필요해서 값으로 저장하지 않고 선언 자체를 들고 있다가 호출 시점에 꺼내 씀) — 2단계 조회 필요.
- Module: import로 뽑히는 선언은 `DeclareStatement`/`FunctionDeclareStatement`뿐이고
  (`MethodDeclareStatement`는 애초에 `ClassDeclareStatement` 내부에서만 나옴),
  `FunctionDeclareStatement` 실행 핸들러가 이미 함수도 `environment_.define(name,
  Value(decl))`로 "그냥 스코프의 값"으로 취급하므로, var든 Func든 moduleScope
  하나에 나란히 들어간다 — `moduleScope->get(name)` 한 번으로 끝나고 별도 목록이
  필요 없다.

**해야 할 일**:
- [ ] Architecture.md §7.3에 위 차이를 명시하는 문장 추가(Module은 Class보다 단순한
      구조라는 점을 오해 없이 밝히기)
- [ ] Implement.md §5의 "§4.3와 사실상 같은 코드 경로" 문구를 위 설명으로 보강

## 3. Assembler: import + alias 접근을 이어붙인 통합 테스트 없음

`AssemblerImportTest`는 `ImportStatement` 노드 자체(declarations 구성, 순환/파일없음/
비선언 오류)만 검증하고, `MethodCallExpressionTest`/`FieldAccessExpressionTest`는
`r.move(5)`/`r.speed` 같은 일반 식별자로만 `FieldAccessExpression`/`CallExpression`
구조를 검증한다. **import로 만든 alias를 바로 이어서 `.`으로 접근하는 시나리오를
한 입력 안에서 함께 검증하는 테스트는 아직 없다.**

```
import "sum.txt" alias sum;
sum.func();
```

를 한 번에 `assemble()`했을 때 `[ImportStatement(...), ExpressionStatement(
CallExpression(FieldAccessExpression(IdentifierExpression("sum"), "func"), []))]`가
나오는지 확인하는 테스트가 없음(파서 구조상 아마 정상 동작하겠지만, 회귀 방지용으로
추가해두는 게 안전하다).

**해야 할 일**:
- [ ] `AssemblerTest.cpp`(또는 `AssemblerImportTest` 스위트)에 위 시나리오 테스트 추가

## 4. 현재 알려진 실패 테스트 (원인 미조사)

`RunPromptShellIntegrationTest`에 아래 4개가 현재 실패 중이다(이번 대화에서 손댄
범위 밖의 변경으로 추가된 테스트로 보임 - 이름상 미구현 기능을 미리 문서화해둔
"red" 테스트일 가능성이 있으나 확인 필요).

- [ ] `LogicalAnd_PrintsFalseWhenLeftOperandIsFalse`
- [ ] `LogicalOr_PrintsTrueWhenLeftOperandIsFalseButRightIsTrue`
- [ ] `DuplicateDeclarationInsideIfBlock_ShouldFailCheckButCurrentlyDoesNot`
- [ ] `SelfReferenceInsideIfBlock_ShouldFailCheckButCurrentlyDoesNot`

원인 파악 후 의도된 red 테스트라면 이 항목은 지우고, 실제 버그라면 별도 항목으로
분리한다.
