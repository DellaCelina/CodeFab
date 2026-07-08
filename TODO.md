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

# Integration test

Architecture.md/3일차 PDF에 기술된 기능들이 실제로 구현되고 나면
`RunPromptShellIntegrationTest`(Tokenizer+Assembler+Checker+Executor를 전부 실제
구현으로 붙여서 소스 → 최종 출력/에러 메시지까지 검증하는 스위트)에 추가할 만한
시나리오를 정리한다. **아래는 시나리오 설명일 뿐 실제 테스트 코드는 아니다** —
각 기능이 구현된 뒤 이 목록을 기준으로 `TEST_F(RunPromptShellIntegrationTest, ...)`
형태로 작성한다.

## 1. Function

- **함수 선언 + 호출 + return 값 사용**: `Func add(a, b) { return a + b; } print add(3, 4);`
  → `7` 출력.
- **재귀 호출**: `Func fact(n) { if (n <= 1) return 1; return n * fact(n - 1); } print fact(5);`
  → `120` 출력. (재귀 시 함수 이름이 호출 전에 이미 스코프에 등록되어 있는지 확인하는
  핵심 시나리오.)
- **return 없이 끝나는 함수**: `Func noop() { } print noop();` → `nil` 출력.
- **매개변수로 받은 값이 호출부에 영향 없음(값 전달)**: 함수 안에서 파라미터를
  재대입해도 호출자의 변수는 바뀌지 않는지 확인.
- **에러: 함수 밖에서 return**: `return 5;` (최상위) → Checker가 의미 오류로 잡아
  Executor까지 도달하지 않아야 한다.
- **에러: 파라미터 이름 중복**: `Func foo(a, a) { }` → 의미 오류.
- **에러: 인자 개수 불일치**: `Func add(a, b) { return a + b; } add(1);` → 런타임(또는
  Checker) 오류.
- **에러: 함수가 아닌 값을 호출**: `var x = "hi"; x();` → `ExecutorError`("호출할 수
  없는 대상입니다" 류).

## 2. Class

- **인스턴스 생성 + 필드 read/write**: `Class Robot { } var r = Robot(); r.speed = 10;
  print r.speed;` → `10` 출력.
- **생성자(init) 사용**: `Class Robot { init(name) { This.name = name; } }
  var r = Robot("AndOr"); print r.name;` → `AndOr` 출력.
- **메서드 호출 + this로 필드 갱신**: `Class Robot { move(dist) { This.position =
  This.position + dist; } } var r = Robot(); r.position = 0; r.move(5);
  print r.position;` → `5` 출력.
- **메서드 안에서 다른 메서드 호출**: `This.report();`처럼 this를 통해 메서드
  체이닝하는 시나리오.
- **instanceof(§5와 연결)**: `Class Robot { } var r = Robot(); print (r instanceof
  Robot);` → `true`.
- **에러: 클래스 외부에서 This 사용**: 최상위 코드에서 `This`를 참조 → 의미 오류.
- **에러: init에서 값 있는 return**: `Class Robot { init() { return 5; } }` → 의미
  오류("init은 return 없음").
- **에러: 존재하지 않는 필드/메서드 접근**: `var r = Robot(); print r.notExist;` →
  런타임 오류.
- **에러: 인스턴스가 아닌 값의 필드 접근**: `var x = "hello"; x.field = 1;` → 런타임
  오류.

## 3. 정적 배열

- **생성 + 인덱스 read/write**: `var arr = Array(3); arr[0] = 10; arr[1] = 20;
  print arr[0];` → `10` 출력.
- **변수를 인덱스로 사용**: `var i = 2; arr[i - 1] = 7; print arr[1];` → `7` 출력.
- **생성 직후 기본값 확인**: `var arr = Array(3); print arr[2];` → `nil` 출력(초기화
  안 된 원소).
- **에러: 범위를 벗어난 인덱스**: `var arr = Array(3); print arr[5];` → 런타임 오류.
- **에러: 인덱스가 숫자가 아님**: `print arr["hello"];` → 런타임 오류.
- **에러: 배열이 아닌 값 인덱싱**: `var x = 10; print x[0];` → 런타임 오류.
- **에러: 배열 크기가 숫자가 아님**: `var arr = Array("hi");` → 런타임 오류.

## 4. Import / Module

- **기본 import + 함수 접근**: `sum.txt`(`Func add(a, b) { return a + b; }`)를
  `import "sum.txt" alias sum;`으로 들여온 뒤 `print sum.add(1, 2);` → `3` 출력.
- **import + 변수 접근**: `lib.txt`(`var pi = 3;`)를 `alias math`로 들여온 뒤
  `print math.pi;` → `3` 출력.
- **같은 import를 다른 블록 스코프에서 각각 사용**: 두 개의 독립된 `{ }` 블록에서
  각자 같은 파일을 import해도 정상 동작(§7.2 "정상" 케이스).
- **에러: 순환 import**: `a.txt`가 `b.txt`를 import하고 `b.txt`가 다시 `a.txt`를
  import → `AssemblerError`.
- **에러: 대상 파일 없음**: `import "missing.txt" alias m;` → `AssemblerError`.
- **에러: 같은 스코프 내 중복 import**: 같은 블록에서 동일 경로를 두 번 import →
  의미 오류.
- **에러: 상위 스코프에서 이미 import한 경로를 하위 스코프에서 재import**: 의미
  오류.
- **에러: 반복문 바디 내부에서 import**: `for (...) { import "x.txt" alias x; }` →
  의미 오류.
- **에러: import 대상 파일에 선언 외의 문장이 섞여 있음**: `print 1;`만 있는 파일을
  import → `AssemblerError`.

## 5. 타입 검사 연산자 (instanceof)

- **정상 - true**: 같은 클래스로 생성한 인스턴스 비교.
- **정상 - false**: 인스턴스가 아닌 값(`3 instanceof Robot`)이거나 다른 클래스의
  인스턴스인 경우.

## 6. 실행 전 최적화 (동작이 바뀌면 안 된다는 회귀 시나리오)

최적화는 "성능만 개선하고 관찰 가능한 동작은 하나도 바꾸지 않아야" 하므로, 아래
시나리오들은 **최적화 켜기 전/후로 출력이 완전히 동일해야 한다**는 것을 검증하는
회귀 테스트다.

- **깊게 중첩된 블록에서의 변수 참조**: `{ { { { var a = 1; print a; } } } }`처럼
  중첩 블록 안에서 변수를 참조해도 정적 바인딩 적용 여부와 무관하게 같은 값을
  출력해야 한다.
- **쉐도잉이 있는 상태에서의 정적 바인딩**: 같은 이름의 변수가 여러 스코프에 걸쳐
  선언된 상태(`var x = "outer"; { var x = "inner"; print x; }`)에서 정적 바인딩이
  엉뚱한 스코프를 가리키지 않는지 확인.
- **상수처럼 보이지만 0으로 나누는 식은 폴딩되면 안 됨**: `print 1 + (3 / 0);`처럼
  피연산자가 전부 리터럴이어도 여전히 런타임에 "0으로 나눌 수 없습니다" 오류가
  나야 한다(컴파일 시점에 조용히 사라지면 안 됨).
- **함수 호출이 섞인 식은 폴딩 대상에서 제외**: `print 1 + add(2, 3);`처럼 리터럴이
  아닌 하위식이 섞여 있으면 그 부분은 그대로 남아있어야 한다(트리 구조 검증은
  Assembler/Checker 단위 테스트 쪽에서, 여기서는 최종 출력값만 확인).

## 7. 공장 제어 쉘 (File / Debug 모드)

Shell의 File/Debug 모드가 구현되면, 기존 REPL 통합 테스트와 별개로 CLI 실행
자체를 검증하는 시나리오가 필요하다(Mock 기반 `FileRunModeTest`/`DebugModeTest`와
별개로, 실제 파일 + 실제 4-Unit을 붙인 통합 시나리오).

- **파일 모드 정상 실행**: 여러 줄짜리 스크립트 파일을 `run` 모드로 실행하면 REPL과
  동일한 최종 출력이 나와야 한다.
- **파일 모드 - 파일 없음**: 존재하지 않는 경로를 `run` 모드로 실행하면 사용자
  친화적 오류 메시지를 출력하고 0이 아닌 종료 코드를 반환해야 한다.
- **파일 모드 - 실행 중 오류 발생 시 즉시 종료**: 중간에 런타임 오류가 나는 스크립트를
  실행하면, REPL처럼 다음 줄을 계속 받지 않고 오류 메시지 출력 후 그 자리에서
  종료해야 한다.
- **디버그 모드 - breakpoint에서 정지**: 특정 줄에 breakpoint를 걸고 `continue`하면
  그 줄에서 정확히 멈추고 이후 줄은 실행되지 않은 상태여야 한다.
- **디버그 모드 - step/next 동작 차이**: `step`은 블록 내부까지 한 문장씩 들어가고,
  `next`는 같은 깊이로 돌아올 때까지는 멈추지 않는지 확인.
- **디버그 모드 - watch 출력**: `watch` 등록 후 매 정지 시점마다 해당 변수의 현재
  값이 출력에 포함되는지 확인.
- **디버그 모드 - inspect**: 현재 스코프의 모든 변수가 출력되는지 확인.
