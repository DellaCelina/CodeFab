# TODO

이 문서는 설계/구현 논의 중 발견된, **아직 Architecture.md/Implement.md/코드 어디에도
확정된 해법이 없는 빈틈**을 추적한다. 실제로 손을 대기 전에 여기 목록을 먼저 확인한다.

## 1. (해결됨, 문서만 미갱신) Executor: Import 실행 시 moduleScope가 채워지지 않는 문제

**현재 상태**: 코드는 이미 고쳐져 있다. `Executor.cpp`의 `ImportStatement` 핸들러는
`popScope()`의 반환값을 옮기는 방식이 아니라, `ScopeGuard`로 임시 프레임을 연 채로
각 선언을 `execute()`한 직후 바로 `moduleScope->define(name,
*environment_.lookup(name))`로 값을 복사해 넣고, 그 다음에 임시 프레임이 스코프를
빠져나가며 pop되는 방식으로 구현되어 있다(즉 "지워지기 전에 옮겨 담는다" 방향으로
해결됨 — 애초에 제안했던 `popScope()` 반환 타입 변경은 필요 없었다).
`FieldAccessExpression`/`CallExpression`(`callMethod`)에도 `object.isModule()`
분기가 이미 들어가 있어 `sum.func()`/`math.pi` 접근이 정상 동작한다.

**남은 문제**: `Implement.md` §5(Executor 담당자 가이드, "할 일 5: import 실행")의
의사코드는 여전히 옛날 버전 그대로다 - `environment_.pushScope()` → 선언 실행 →
`environment_.popScope();`(반환값 버림, moduleScope는 계속 빈 상태)로 되어 있어
실제 구현과 다르다. 문서만 실제 코드를 못 따라간 상태.

**해야 할 일**:
- [ ] `Implement.md` §5(Executor)의 import 실행 의사코드를 `Executor.cpp`의 실제
      구현(ScopeGuard 안에서 실행 직후 `moduleScope->define`으로 즉시 복사)에 맞게
      갱신
- [ ] `Architecture.md` §7.3에도 moduleScope 구성 방식을 실제 구현 기준으로 명시

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

## 12. Executor: 클래스 메서드에 대한 재귀 호출 integration test 없음

**증상**: `IntegrationTest/DebugIntegrationTest.cpp`에는 함수 재귀 호출 테스트
(`RecursiveCall_ComputesFactorial`, `Func fact(n) { ... }`)는 있지만, **메서드**를
`This.method(...)` 형태로 재귀 호출하는 시나리오는 없다. 함수 재귀와 메서드 재귀는
이름 바인딩 경로가 다르다 - 함수는 `environment_.define(name, Value(decl))`로
호출 전에 이미 스코프에 등록돼 있어야 하는 반면, 메서드는 `klass->methods` 목록에서
매 호출마다 이름으로 찾고 `this`를 새로 바인딩해서 실행하므로, 재귀 중에 `this`가
올바른 인스턴스를 계속 가리키는지, 그리고 메서드 탐색이 매 재귀 호출마다 정상
동작하는지는 별도로 검증해야 한다.

**해야 할 일**:
- [ ] `IntegrationTest/DebugIntegrationTest.cpp`(`RunPromptShellIntegrationTest`
      스위트)에 메서드 재귀 호출 테스트 추가, 예:
      `Class Calc { fact(n) { if (n <= 1) return 1; return n * This.fact(n - 1); } }
      var c = Calc(); print c.fact(5);` → `120` 출력
- [ ] 재귀 도중 `This`가 항상 같은 인스턴스를 가리키는지 확인하는 케이스도 함께
      추가(예: 재귀 중 필드를 누적시키는 메서드를 만들어 최종 필드 값으로 검증)

## 13. Executor/Import: module에 대한 integration test 없음

**증상**: `IntegrationTest/DebugIntegrationTest.cpp`에는 import/module 관련 테스트가
전혀 없다("import"/"module" 문자열 자체가 등장하지 않음). Import 실행 경로 자체는
고쳐져 있고(위 "# TODO"의 1번 항목 참고) `FieldAccessExpression`/`CallExpression`의
module 분기도 구현되어 있으므로, 이제는 end-to-end로 검증할 차례다.

**의존성**: 테스트용 module 파일에 `Class` 선언까지 넣으려면, `Assembler.cpp`의
`parseImport`가 현재 `DeclareStatement`/`FunctionDeclareStatement`만 허용하고
`ClassDeclareStatement`가 섞이면 `AssemblerError`를 던지는 제약과 부딪힌다(위
"# TODO"의 9번 항목 참고). 클래스 import를 지원하기로 결정하기 전까지는, module
테스트 파일에 `Class` 선언을 넣어 두더라도 그 파일을 실제로 import하는 테스트는
9번 항목의 "클래스 import 허용" 결정이 먼저 나야 통과할 수 있다는 점을 감안해서
작성 순서를 잡는다(예: `Func`/`var`만 있는 module로 우선 import 시나리오를
완성하고, `Class`가 섞인 module은 9번 항목 결정 이후로 미룬다).

**해야 할 일**:
- [ ] `IntegrationTest` 폴더 아래에 테스트 전용 module 파일을 2개 이상 작성한다.
      각 파일에는 `Func`, `Class`, `var` 선언이 모두 들어 있어야 한다(단, 실제로
      import해서 통과시키는 테스트는 위 의존성 문단대로 `Class`를 뺀 하위 집합부터
      단계적으로 추가).
- [ ] 기본 module import 시나리오 테스트: 함수/변수 접근(`sum.add(1, 2)`,
      `math.pi`)이 정상 동작하는지 확인
- [ ] scope 변화에 따른 module import 테스트: 서로 다른 블록 스코프에서 같은
      module을 각각 import했을 때 서로 간섭하지 않는지, 블록을 벗어나면 alias가
      더 이상 보이지 않는지 확인
- [ ] `if`절 등 조건문/블록 내부에서의 module import 테스트: 조건이 참일 때만
      import가 실행되고 그 alias가 해당 블록 스코프에만 존재하는지 확인
- [ ] 2개 이상의 module을 동시에 import했을 때 생길 수 있는 문제를 테스트로
      확인: 서로 다른 module에 동일한 이름(`Func`/`var` 이름)이 있어도 각각 다른
      alias로 접근하면 충돌하지 않는지, 두 module을 같은 스코프에서 import한 뒤
      각 alias로 교차 접근했을 때 값이 서로 섞이지 않는지
- [ ] 위 "# TODO" 1번(문서 갱신)·9번(클래스 import 결정) 항목의 진행 상황에 맞춰
      이 테스트 목록을 갱신

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

## 5. 클래스 상속(Super, `:`)이 Architecture.md/Implement.md에 설계만 있고 미확정

3일차 PDF에는 상속(`Class B : A`), `Super` 키워드로 상위 클래스 메서드 호출, 메서드
오버라이딩, `instanceof`가 부모 클래스에 대해서도 `true`를 반환해야 한다는 요구사항이
들어있다. 그런데 Architecture.md §4.5는 "상속 확장 지점(이번 범위 제외)"라는 제목
그대로, 나중에 추가할 때 손댈 지점만 짧게 나열해둔 **스텁(stub)**이고, 실제 문법/의미
검사/실행 규칙은 하나도 확정되어 있지 않다. Implement.md에는 상속 관련 내용이 아예
없다(Assembler/Checker/Executor 담당자 가이드 어디에도 `Super`/`SuperExpression`/
`superclass`가 등장하지 않음).

**해야 할 일** (아래 항목들이 먼저 정리되어야 "# Integration test"의 §2-1 상속
시나리오를 실제 테스트로 옮길 수 있다):

- [ ] Architecture.md §2.1에 `SUPER`, `COLON`(`:`) 토큰 추가하고, §2.2에
      `ClassDeclareStatement.superclass: IdentifierExpression*`(nullable) 필드와
      `SuperExpression(keyword, method)` 노드를 정식으로 추가(지금은 §4.5에
      아이디어만 있고 노드 스펙이 없음).
- [ ] Assembler 파싱 규칙 확정: `classDeclStmt -> CLASS IDENTIFIER (COLON IDENTIFIER)?
      LEFT_BRACE methodDeclStmt* RIGHT_BRACE`, `Super.method(...)` 파싱(우변이 항상
      메서드 호출 형태로 강제되는지, 아니면 `Super.field`처럼 필드 접근도 허용하는지
      팀 결정 필요) — Implement.md §2(Assembler)에 의사코드 추가.
- [ ] Checker 의미 검사 확정: 자기 자신 상속(`Class A : A`) 금지, 클래스가 아닌 대상
      상속 금지, 클래스 외부에서 `Super` 사용 금지, **부모 없는 클래스에서 `Super`
      사용 금지** — Implement.md §3(Checker)에 추가.
- [ ] Executor 실행 규칙 확정: 메서드 탐색을 `klass->methods` → 없으면
      `klass->superclass->methods`로 재귀 확장(오버라이딩은 이 탐색이 자식 클래스부터
      먼저 찾으므로 자연히 해결됨), `Super.method(...)` 호출은 **현재 인스턴스(this)는
      그대로 유지한 채** 메서드 탐색 시작점만 `this`가 속한 클래스가 아니라
      `superclass`로 강제 이동 — Implement.md §4(Executor)에 추가.
- [ ] `InstanceOfExpression` 평가 로직을 "정확히 같은 클래스인지"(포인터 비교 1회)에서
      "`instance.klass`부터 `superclass` 체인을 따라 올라가며 일치하는 게 있는지"로
      확장(Architecture.md §8.2, Implement.md §4 "할 일 4" 갱신).
- [ ] 필드 저장소(`InstanceValue.fields`)는 상속 여부와 무관하게 인스턴스당 하나로
      계속 충분한지 재확인(Architecture.md §4.5는 "충분하다"고 되어 있으나, 부모
      클래스의 `init`이 자식과 다른 필드를 설정하는 경우까지 고려했을 때 실제로
      문제없는지 시나리오로 검증 필요 — 아래 Integration test §2-1 참고).

## 8. Assembler: `makeBinaryExpression`의 `default` 분기가 `OrExpression`으로 암묵적 하드코딩됨

**증상**: `Assembler.cpp:433-459`의 `makeBinaryExpression`은 각 연산자 토큰을 대응하는
`Expression` 노드로 매핑하는데, `AND`만 `case TokenType::AND:`로 명시적으로 처리하고
그 외 나머지 전부를 처리하는 `default:` 분기가 곧바로 `addNode<OrExpression>(...)`를
반환한다.

```cpp
case TokenType::PERCENT: return addNode<ModExpression>(Tokens{ opToken }, left, right);
case TokenType::AND: return addNode<AndExpression>(Tokens{ opToken }, left, right);
default: return addNode<OrExpression>(Tokens{ opToken }, left, right);
```

지금은 `kDefaultOperatorPriority`가 이 분기까지 내려보내는 토큰이 실질적으로 `OR`
하나뿐이라 우연히 정답이 나온다. 하지만 이후 새 이항 연산자를 우선순위 테이블에
추가하면서 여기 `switch`에 `case`를 추가하는 걸 깜빡하면, 그 새 연산자는 조용히
`OrExpression`으로 파싱되어 디버깅하기 매우 어려운 버그가 된다(파싱은 성공하고,
겉보기엔 그럴듯한 AST가 만들어지기 때문).

**해야 할 일**:
- [ ] `case TokenType::OR:`를 명시적으로 추가해 `OrExpression`을 반환하도록 변경
- [ ] `default:`는 `assert(false)` 또는 `AssemblerError`를 던지도록 변경해, 우선순위
      테이블과 `switch`가 어긋나는 순간 바로 드러나게 함

## 9. Import 가능한 선언 종류가 Assembler와 Executor에서 서로 다름

**증상**: `Assembler.cpp:284`(`parseImport`)는 import 대상 파일에서
`DeclareStatement`/`FunctionDeclareStatement`만 허용하고, 그 외(예: `ClassDeclareStatement`)가
섞여 있으면 `AssemblerError`를 던진다.

```cpp
if (!dynamic_cast<DeclareStatement*>(decl) && !dynamic_cast<FunctionDeclareStatement*>(decl)) {
    throw AssemblerError("import 대상 파일에는 선언 외의 내용을 허용하지 않습니다: '{}'", pathToken.origin);
}
```

반면 `Executor.cpp:26-37`(`declaredNameOf`)은 `ClassDeclareStatement`까지 이름을
뽑아낼 수 있도록 분기가 이미 만들어져 있다. 즉 Executor는 "모듈에서 클래스를
import해서 이름을 등록하는" 경로를 준비해 뒀는데, 그 앞단인 Assembler가 애초에
그런 트리를 만들지 못하게 막고 있어 `declaredNameOf`의 클래스 분기가 죽은 코드다.
클래스 import를 정식으로 지원할지, 아니면 var/Func만 허용하는 현재 설계가 맞는지
팀 결정이 되어 있지 않은 상태로 두 모듈의 구현이 어긋나 있다.

**해야 할 일**:
- [ ] 클래스 import 지원 여부를 팀 결정(Architecture.md §7.x에 명시)
- [ ] 지원한다면 `Assembler.cpp:284`의 허용 목록에 `ClassDeclareStatement` 추가
- [ ] 지원하지 않는다면 `Executor.cpp`의 `declaredNameOf`에서 `ClassDeclareStatement`
      분기 제거(도달 불가능한 죽은 코드 정리)
- [ ] 결정된 내용을 `ExecutorImportTest.cpp`/`AssemblerImportTest`에 회귀 테스트로 추가

## 11. Executor: RTTI(`typeid`/`dynamic_cast`) 대신 Visitor 패턴으로 리팩토링

**현재 상태**: `SyntaxNode`(`SyntaxTree.h:14` 부근)의 가상 멤버는 소멸자와
`operator==`뿐이고, `accept()`/`Visit` 계열 메서드가 전혀 없다. 그 결과:

- `Executor.cpp`는 `std::type_index(typeid(*stmt))`/`typeid(*expr)`를 키로 하는
  `statementHandlers_`/`expressionHandlers_` 맵으로 1차 분기한 뒤, 핸들러 내부에서
  다시 `static_cast`/`dynamic_cast`로 구체 타입을 확정하는 방식으로 동작한다
  (`Executor.cpp:161,171,178,305` 등에서 `dynamic_cast` 사용).
- `Checker.h:40`에는 "SyntaxNode에 accept()가 없어 dynamic_cast로 타입 분기한다"는
  주석이 이미 명시적으로 남아 있어, RTTI 기반 분기가 임시방편이라는 인식은 팀 내에
  이미 있었던 것으로 보인다.

RTTI 기반 분기는 새 노드 타입이 추가될 때 `typeid` 키 등록을 빠뜨려도 컴파일
에러 없이 조용히 "처리 안 됨" 상태로 넘어갈 수 있고, 분기 로직이 맵 초기화 코드와
캐스팅 코드 두 군데로 흩어져 있어 노드 하나를 다루는 코드가 한눈에 보이지 않는다.

**해야 할 일**:
- [ ] `SyntaxTree.h`에 `SyntaxNodeVisitor` 인터페이스를 정의하고, `Statement`/
      `Expression` 계열의 각 구체 노드(`DeclareStatement`, `FunctionDeclareStatement`,
      `ClassDeclareStatement`, `ImportStatement`, `IfStatement`, `ForStatement`,
      `BlockStatement`, `ReturnStatement`, `PrintStatement`, `ExpressionStatement`,
      `BinaryExpression` 계열, `CallExpression`, `FieldAccessExpression`,
      `IdentifierExpression`, `ArrayExpression`, `InstanceOfExpression` 등 전부)에
      대응하는 `visit(...)` 오버로드를 선언
- [ ] `SyntaxNode`(및 `Statement`/`Expression` 중간 기반 클래스)에 순수 가상
      `accept(SyntaxNodeVisitor&)`를 추가하고, 각 구체 노드가 자기 타입을 인자로
      `visitor.visit(*this)`를 호출하도록 구현(더블 디스패치)
- [ ] `Executor`가 `SyntaxNodeVisitor`를 상속하도록 변경하고, 기존
      `statementHandlers_`/`expressionHandlers_` 맵과 그 안의 람다들을 각 `visit(...)`
      오버라이드 메서드로 옮겨 구현
- [ ] `execute(Statement*)`/`evaluate(Expression*)` 진입점을 `node->accept(*this)`
      호출로 교체하고, 반환값이 필요한 `evaluate` 쪽은 현재 값을 담아둘 멤버 변수
      (또는 방문 결과를 리턴하는 별도 매커니즘)를 어떻게 둘지 설계
- [ ] 남아있는 `dynamic_cast` 분기(예: 값 타입 판별용이 아니라 노드 타입 판별용으로
      쓰인 것들)를 visitor 진입 이후에는 제거할 수 있는지 확인
- [ ] `Checker`는 이번 리팩토링 범위에서 제외하고 Executor만 우선 전환하되, `Checker.h:40`
      주석에 "Executor는 Visitor로 전환됨, Checker는 아직 dynamic_cast 사용 중"이라고
      현재 상태를 남겨 향후 동일 리팩토링 필요성을 표시
- [ ] Architecture.md/Implement.md의 Executor 관련 서술을 RTTI 기반 설명에서 Visitor
      패턴 기반으로 갱신

## 10. Checker와 Optimizer(상수 폴딩)를 SRP에 맞게 분리하는 리팩토링 (PR 논의 정리)

**배경**: Architecture.md의 상수 폴딩(§6.2) 설계 PR([#24](https://github.com/DellaCelina/CodeFab/pull/24))
리뷰에서 나온 논의다. 현재 설계는 `Checker`가 생성자에서 `ExecuteInterface&`(정확히는
`ExecutorInterface`)를 주입받아 의미 오류 검사와 상수 폴딩(값 계산)을 함께 수행한다.

- 의존성 방향: `Checker`가 구체 `Executor`가 아니라 `ExecutorInterface`(추상화)에
  의존하므로, Checker → Executor로의 역방향 의존이 아니라 DIP(의존성 역전)를 만족하는
  형태로 설계되어 있다 - 이 부분은 이미 정리된 결론.
- 책임 확장 문제: 원래 "의미 오류만 검사"하던 `Checker`가 "상수 계산(값 평가)"까지
  맡게 되어 SRP 관점에서 책임이 두 개로 늘어난 상태다.
- 테스트 비용: `Checker`를 테스트할 때마다 Fake `ExecuteInterface`가 필요해지는데,
  이는 Fixture로 어느 정도 완화 가능하다는 의견까지만 나오고 구체적인 해법은
  확정되지 않았다.
- 현재는 문서(Architecture.md)에 이미 기술된 대로 "Checker 안에서 처리"하는 방식으로
  우선 구현하고, 아래 분리는 추후 리팩토링 과제로 미룬 상태다.

**제안된 방향(팀 합의, 상세 설계는 미확정)**:
- 오류 검사는 `Checker`, 상수 계산/최적화는 별도 `Optimizer`로 책임 분리.
- 상수 계산 로직 자체는 이미 `Executor`가 담당하고 있으므로, `Optimizer`는
  `Checker`와 동일하게 `ExecutorInterface`에 의존하는 편이 DIP를 유지하는 데
  자연스럽다.

**해야 할 일**:
- [ ] `Checker`/`Optimizer` 분리 시점과 범위를 팀 논의로 확정(다음 스프린트 vs
      상수 폴딩 기능 안정화 이후 등)
- [ ] `Optimizer`가 맡을 책임의 경계를 Architecture.md에 명시(상수 폴딩 외에 추가로
      가져갈 최적화가 있는지 포함)
- [ ] 분리 후 `Checker`/`Optimizer` 각각이 필요로 하는 `ExecutorInterface` 의존
      방식(생성자 주입 등)과 테스트용 Fake/Fixture 구조를 재설계
- [ ] Architecture.md/Implement.md에 분리된 구조로 갱신

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
- **동적 필드 추가**: `Class Robot { }`처럼 필드를 하나도 선언/초기화하지 않는(즉
  `init`도 없는) 클래스로 인스턴스를 만든 뒤, `var r = Robot(); r.speed = 10;
  print r.speed;`처럼 생성 이후 처음 보는 이름에 바로 대입하면 그 필드가 새로
  생겨서 읽을 수 있어야 한다(3일차 요구사항: "없는 필드일 경우 새로 생성"). 아래
  두 케이스를 모두 확인한다.
  - `init`이 전혀 없는 클래스에 필드를 동적으로 추가하는 경우.
  - `init`이 일부 필드만 설정해둔 클래스에서, `init`이 건드리지 않은 **새 이름**의
    필드를 생성 이후에 동적으로 추가하는 경우(`Class Robot { init(name) {
    This.name = name; } } var r = Robot("A"); r.speed = 5; print r.name + "," +
    r.speed;` → `A,5`). `init`이 설정한 필드와 동적으로 추가한 필드가 같은
    인스턴스에 공존해야 한다.

## 2-1. 클래스 상속 (§5 "TODO" 목록에서 설계가 먼저 확정되어야 함)

**주의**: 아래 시나리오는 3일차 PDF의 요구사항이지만, 현재 Architecture.md는
상속을 "이번 범위 제외 + 확장 지점만 기록"으로 남겨두고 있어 문법(`Super`, `:`)과
실행 규칙이 아직 확정되어 있지 않다(위 "# TODO"의 5번 항목 참고). 아래 시나리오는
그 설계가 끝난 뒤에 구현 순서대로 테스트로 옮긴다.

- **메서드 오버라이딩**: 부모 클래스 `Robot`과 자식 클래스 `SpeedRobot : Robot`이
  같은 이름의 메서드를 각각 정의했을 때, 자식 인스턴스에서 그 메서드를 호출하면
  **자식의 구현이 실행**되어야 한다(`Class Robot { move(dist) { print "move"; } }
  Class SpeedRobot : Robot { move(dist) { print "speed move"; } }
  SpeedRobot().move(3);` → `speed move` 출력).
- **오버라이드하지 않은 메서드는 부모 것을 그대로 상속**: 자식 클래스가 재정의하지
  않은 메서드를 자식 인스턴스에서 호출하면 부모의 구현이 실행되어야 한다.
- **Super로 부모 메서드 호출**: 자식이 메서드를 오버라이드하면서 그 안에서
  `Super.move(dist)`로 부모의 구현을 호출하면, 오버라이드된 자식 구현이 아니라
  **부모의 구현이 실행**되어야 한다(`Class SpeedRobot : Robot { move(dist) {
  Super.move(dist); print "speed!"; } }` 호출 시 `move`(부모 출력) → `speed!` 순서로
  출력).
- **Super로 호출해도 필드는 같은 인스턴스를 가리킴**: `Super.move(dist)`가 부모
  구현을 실행하는 동안 `This.position`처럼 필드에 접근하면, 별도의 부모 인스턴스가
  아니라 **원래 호출한 자식 인스턴스와 같은 필드 저장소**를 읽고 써야 한다.
- **다단계 상속(조부모까지)**: `Class C : B`, `Class B : A`처럼 2단계 이상 상속했을 때
  `C`의 인스턴스가 `A`에만 있는 메서드도 호출할 수 있어야 한다.
- **instanceof가 부모 클래스에 대해서도 true**: `Class Robot { } Class SpeedRobot :
  Robot { } var w = SpeedRobot(); print (w instanceof SpeedRobot);` → `true`,
  `print (w instanceof Robot);` → **`true`**(자기 자신뿐 아니라 부모 클래스여도
  성립해야 한다는 게 핵심 - 단순 포인터 비교 1회로는 이 케이스를 못 잡는다).
- **instanceof가 관계 없는 클래스에는 false**: `SpeedRobot` 인스턴스가 `SpeedRobot`/
  `Robot`과 상속 관계가 없는 별도의 `Class Other { }`에 대해서는 `false`를 반환해야
  한다.
- **에러: 자기 자신을 상속**: `Class Robot : Robot { }` → 의미 오류.
- **에러: 클래스가 아닌 대상을 상속**: `var x = 10; Class Robot : x { }` → 의미 오류.
- **에러: 클래스 외부에서 Super 사용**: 최상위 코드에서 `Super.move();` → 의미 오류.
- **에러: 부모가 없는 클래스 안에서 Super 사용**: 상속하지 않은 클래스의 메서드
  안에서 `Super.move();` → 의미 오류.

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

## 8. 논리 연산자(`and`/`or`)와 나머지 연산자(`%`)

**주의**: 위 "# TODO"의 7번 항목 참고 - `and`/`or`/`%`는 토큰/AST/실행 로직이
전부 빠져있어서 지금은 파싱 단계부터 실패한다. 아래 시나리오는 그 갭이 채워진
뒤 테스트로 옮긴다.

- **and - 정상**: `print true and true;` → `true`, `print true and false;` →
  `false`, `print false and true;` → `false`, `print false and false;` → `false`.
- **or - 정상**: `print false or true;` → `true`, `print true or false;` →
  `true`, `print false or false;` → `false`.
- **비교/산술 연산자와의 조합**: `print (1 < 2) and (3 > 2);` → `true`,
  `print (1 > 2) or (3 > 2);` → `true`.
- **short-circuit 평가**: `and`는 좌항이 false면 우항을 평가하지 않아야 하고,
  `or`는 좌항이 true면 우항을 평가하지 않아야 한다 - 부작용이 있는 함수 호출을
  우항에 두고(예: 카운터를 증가시키는 함수) 실제로 호출되지 않는지 확인
  (`Func bump() { c = c + 1; return true; } var c = 0; var x = false and bump();
  print c;` → `0`, 우항이 평가됐다면 `1`이 나올 것).
- **에러 정책 결정 필요**: `print 1 and 2;`처럼 피연산자가 Boolean이 아닐 때
  타입 오류를 낼지, 아니면 truthy 기반으로 통과시킬지는 팀 결정 사항 - 결정된
  정책대로 정상/에러 케이스를 확정한다.
- **% - 정상**: `print 7 % 3;` → `1`, `print 10 % 5;` → `0`, `print 2 * 3 % 4;` →
  `2`(우선순위가 `*`/`/`와 같은 레벨인지, 좌결합인지 확인).
- **% - 최적화 슬라이드 예시 재현**: `print (1 - 2 * 3 * 4 * 5 / 6 + 7 + 8 + 9) %
  1000 % 30;`처럼 여러 연산자가 섞인 식에서 `%`가 마지막에 두 번 연속 적용되는
  경우까지 정확한 값이 나오는지 확인(§6 실행전 최적화가 이 식을 상수 폴딩할 때도
  동일한 값을 내야 한다는 회귀 테스트와도 연결됨).
- **에러: 0으로 나머지 연산**: `print 5 % 0;` → `DivideExpression`과 동일하게
  런타임 오류("0으로 나눌 수 없습니다" 류).

# 코드 정리

## 1. 에러 메시지 포맷 통일 (한글 → 영어)

현재 모듈별로 에러 메시지 언어가 혼재되어 있다. 일부는 한글, 일부는 영어로 출력된다.
전체 에러 메시지를 영어로 통일해야 한다.

**해야 할 일**:
- [ ] 각 모듈(`Tokenizer`, `Assembler`, `Checker`, `Executor`)의 에러 메시지를 전수 조사
- [ ] 한글로 작성된 에러 메시지를 영어로 변환
- [ ] 에러 메시지 형식(포맷)도 모듈 간 일관성 있게 통일

## 2. 주석 정리

코드와 일치하지 않는 주석, 또는 불필요한 주석이 존재할 수 있다.

**해야 할 일**:
- [ ] 전체 코드베이스에서 코드와 내용이 맞지 않는 주석 파악 및 수정
- [ ] 코드만으로 충분히 이해되는 불필요한 주석 제거

## 3. `Environment::lookupAt`/`assignAt`에 범위 검사 없음 (master 병합 후 실전 위험으로 격상)

`Environment.cpp:42-50`에서 `distance`가 현재 스코프 깊이보다 크면
`size_t index = scopes_.size() - 1 - distance;`가 언더플로우로 거대한 값이 되고,
뒤이은 `scopes_[index]`는 `std::vector::operator[]`라 범위를 벗어나도 예외 없이
크래시/메모리 오염으로 이어진다.

이 항목을 처음 적을 때는 "`IdentifierExpression`에 depth를 채워 넣는 Resolver가
아직 없어 이 경로에 잘못된 `distance`가 들어올 일이 없다"고 적었는데, master에
`Checker::resolveIdentifier`(`Checker.cpp:331-341`)가 merge되면서 depth가 실제로
채워지고 `Executor.cpp:149-151`, `183-185`에서 바로 쓰이기 시작했다 - 즉 더 이상
가상의 위험이 아니라 지금 당장 스코프 계산이 어긋나면 바로 재현되는 위험이다.

실제로 어긋날 수 있는 구체적인 지점 하나가 이미 PR
[#36](https://github.com/DellaCelina/CodeFab/pull/36#discussion_r3541866636)
리뷰에서 지적됐다: `Checker::checkImport`가 import 내부 선언들을 검사할 때
`enterScope()`/`exitScope()` 없이 현재 스코프에서 바로 `checkStatement(decl)`을
호출해서, import 내부 선언 이름이 바깥 스코프로 그대로 새어나간다(`checkBlock`/
`checkFor`의 enterScope + try/catch(exitScope, rethrow) 패턴을 똑같이 적용하자는
해법까지 리뷰에서 나온 상태 - 별도 항목으로 안 만들고 여기서 연결만 해둔다).
Checker와 Executor가 계산하는 스코프 개수가 이렇게 어긋나면, 그 어긋난 만큼
`distance`가 잘못된 값으로 Executor에 전달되고, 방어 코드가 없는 `lookupAt`/
`assignAt`가 그 값을 그대로 써서 범위를 벗어난 접근으로 이어질 수 있다.

**해야 할 일**:
- [ ] `distance`가 `scopes_.size()`보다 크거나 같으면 예외를 던지도록 방어 코드 추가
- [ ] `scopes_[index]` 대신 `scopes_.at(index)`로 바꿔 최소한 UB 대신 명확한 예외가
      나도록 변경(성능이 문제라면 디버그 빌드에서만 `at` 사용 검토)
- [ ] PR #36의 `checkImport` 스코프 누락 수정이 실제로 반영됐는지 확인(Checker
      담당 항목이지만, 이 항목의 전제 조건이라 함께 트래킹)

## 4. 클래스 메서드 탐색 로직 중복 (`instantiate`/`callMethod`)

`Executor.cpp:480-484`(`instantiate`에서 `init` 탐색)과 `Executor.cpp:511-516`
(`callMethod`에서 임의 메서드 탐색)이 `klass->methods`를 선형 탐색하는 거의 동일한
루프를 각자 가지고 있다.

**해야 할 일**:
- [ ] `MethodDeclareStatement* findMethod(const ClassDeclareStatement* klass, const
      std::string& name)` 같은 공용 헬퍼로 통합
- [ ] 두 호출부를 헬퍼 사용으로 교체(상속 도입 시 `superclass` 체인 탐색을 한 곳만
      고치면 되도록 미리 정리 — 위 "# TODO" 5번 상속 항목과 연결)

## 5. 인자 평가(`evaluate(arg)`) 패턴 3곳 중복

`Executor.cpp:312-314`(`CallExpression`), `500-502`(`callMethod` 모듈 분기),
`522-524`(`callMethod` 인스턴스 분기)에서 `for (Expression* arg : argExprs) {
args.push_back(evaluate(arg)); }` 패턴이 그대로 반복된다.

**해야 할 일**:
- [ ] `std::vector<Value> evaluateArgs(const std::vector<Expression*>& argExprs)`
      헬퍼로 통합하고 세 호출부를 교체
</content>
