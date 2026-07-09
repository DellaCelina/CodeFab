# Checker 모듈

Tokenizer → Assembler → **Checker** → (Optimizer) → Executor 파이프라인에서, Assembler가
만든 `SyntaxTree`를 실행 전에 훑어 의미 오류(변수 중복 선언, 자기 참조, 선언되지 않은
변수, 함수/클래스/import 관련 규칙 위반 등)를 찾는다. 오류가 있으면 `CheckerError`를
throw하고, 없으면 `true`를 반환한다.

## 파일 구성

| 파일 | 역할 |
|---|---|
| `CheckerInterface.h` | `CheckerError`, `CheckerInterface::check(SyntaxTree&)` 계약 |
| `Checker.h` / `Checker.cpp` | 의미 오류 검사 구현체 |
| `CheckerTest.cpp` | Checker 단위 테스트 |
| `OptimizerInterface.h` | `OptimizerInterface::optimize(SyntaxTree&)` 계약 |
| `Optimizer.h` / `Optimizer.cpp` | 상수 폴딩(ConstantFolder) 구현체 - Checker와 책임 분리 |
| `OptimizerTest.cpp` | Optimizer 단위 테스트 |

## Checker 동작 개요

- **스코프 스택**: `scopes`(`vector<unordered_set<string>>`)가 REPL 세션 전체에 걸쳐
  유지된다(Executor의 `Environment`와 동일한 이유) - `check()`를 여러 번 호출해도 이전에
  선언한 변수가 계속 "선언된 것"으로 인식된다. `classNames_`가 `scopes`와 나란히
  push/pop되며, 그중 `Class`로 선언된 이름만 따로 기억한다(상속 대상이 클래스인지
  판별하는 데 쓰임).
- **디스패치**: `checkStatement`/`checkExpression`은 `dynamic_cast` if-else 체인이
  아니라 `type_index -> handler` 테이블(`registerDefaultHandlers()`가 채움)로 노드
  타입을 구분한다(Executor와 동일한 패턴). 새 노드 타입이 추가되면
  `registerDefaultHandlers()`에 한 줄만 추가하면 된다. `BinaryExpression`/
  `UnaryExpression`처럼 여러 구체 하위 타입이 같은 처리를 공유하는 경우, 같은 람다를
  각 하위 타입 typeid로 반복 등록한다(type_index는 정확한 타입만 매칭되기 때문).
- **검사 종류**:
  - 변수: 같은 스코프 중복 선언, 초기화식에서의 자기 참조, 선언되지 않은 변수 사용.
  - if/for: `thenBranch`/`elseBranch`/`loop` 내부 블록까지 재귀 검사. for의 `init`은
    전용 스코프에 갇혀 바깥으로 새지 않는다.
  - 함수: 파라미터 이름 중복, 함수 밖 `return`, 재귀 호출을 위해 바디 검사 "전"에
    이름을 스코프에 등록.
  - 클래스: 이름 중복, `This`를 클래스 메서드 밖에서 사용 금지, `init` 메서드의 값 있는
    `return` 금지, **상속**(자기 자신 상속 금지 / 클래스가 아닌 대상 상속 금지 -
    `classNames_`로 판별), `Super`를 클래스 메서드 밖 또는 부모 없는 클래스에서 사용
    금지.
  - import: for 문 내부에서 import 금지, 같은/상위 스코프에서 alias 재사용 금지,
    import 내부 선언은 **전용 스코프**에서 검사해 바깥으로 새지 않는다.
  - Resolver(정적 바인딩): 선언 여부 확인을 통과한 식별자마다 몇 단계 위 스코프에서
    찾았는지 세어 `IdentifierExpression::depth`에 기록한다. 단, 함수/메서드 본문
    안에서는 건너뛴다(재귀 호출 시 실제 런타임 스코프 깊이가 선언 시점 depth와
    달라지기 때문 - `checkIdentifier` 주석 참고).

## Optimizer(ConstantFolder)와의 책임 분리

원래 Checker 안에 있던 상수 폴딩 로직(`foldConstantIfPossible`)을 `Optimizer`로
분리했다. `Optimizer::optimize(tree)`는 **`Checker::check(tree)`가 `true`를 반환한
트리에 대해서만** 호출되어야 하며(`OptimizerInterface.h` 계약), 통과 못하는 경우(예:
`1 / 0`처럼 상수처럼 보이지만 런타임에 0으로 나누기 오류가 나야 하는 식)는 예외를
던지지 않고 원본 서브트리를 그대로 둔다.

- `BinaryExpression::left/right`, `UnaryExpression::operand`가 `Expression* const`에서
  `Expression*`(비-const)로 완화되어 있어, 두 자식이 모두 리터럴이면
  `ExecuteInterface::evaluate()`로 값을 구한 뒤 그 자리를 리터럴 노드로 실제로
  치환한다(bottom-up).
- 다른 필드(`PrintStatement::expr`, `DeclareStatement::expr`, `CallExpression::arguments`
  등)는 여전히 `const`라서, 이런 자리에 바로 놓인 최상위 상수식 자체는 치환되지 않는다
  - 다만 그 아래에 중첩된 `BinaryExpression`/`UnaryExpression`의 non-const 자식
    슬롯은 여전히 실제로 치환된다(`OptimizerTest.cpp`의
    `ReplacesNestedBinaryExpressionWithFoldedLiteral` 참고).
- `Checker`는 여전히 `ExecuteInterface&`를 생성자로 받는다 - 지금은 내부적으로 쓰지
  않지만, Shell 쪽 배선(생성자 시그니처)을 바꾸지 않기 위해 유지했다.

## 알려진 제한사항 (코드의 `TODO(refactor)` 참고)

- **import 동일 파일 재import 검사**: `ImportStatement`에 원본 경로 필드가 없어서
  "동일 path" 대신 "동일 alias 이름"으로 대체 검사한다 - 서로 다른 alias로 같은
  파일을 두 번 import하면 걸러내지 못한다.
- **클래스 내 메서드 이름 중복**: 검사하지 않는다(필수 요구사항 아님).
- **`instanceof` 대상 클래스**: `className`이 `Token`이라 정적으로 선언 여부를
  확인하지 않는다(런타임에 Executor가 확인).
- **Visitor 패턴 미적용**: `SyntaxNode::accept()`가 추가됐지만, Checker는 이번
  라운드 리팩토링 범위가 아니라 여전히 `dynamic_cast`/`type_index` 기반으로 동작한다
  (Executor부터 전환 진행 중).

## 변경 이력

1. **기본 의미 검사** - 변수 중복 선언, 자기 참조, 선언되지 않은 변수 검사 구현.
2. **if/for 블록 내부 검사 누락 수정** - 코드 리뷰로 발견된 버그. `checkStatement`가
   `IfStatement`/`ForStatement`의 내부 블록까지 재귀하도록 수정하고 회귀 테스트 추가.
3. **함수/클래스/import/Resolver 구현** - Architecture.md/Implement.md 3일차 확장
   반영: 함수 선언/재귀/파라미터 중복 검사, 클래스 선언/`This`/`init` 검사, import
   스코프 규칙, 정적 바인딩(Resolver)의 `depth` 계산.
4. **ConstantFolder 호출 검증(1차)** - `BinaryExpression::left/right`가 아직 const라
   실제 트리 치환은 못하고, `executor_.evaluate()`가 올바르게 호출되는지만 Mock으로
   검증하는 형태로 우선 추가.
5. **주석 정리** - 장황한 설명형 주석을 팀원이 읽기 편한 짧은 주석으로 정리.
6. **`dynamic_cast` if-else 체인 → `type_index` 디스패치 테이블 리팩토링** -
   Executor와 동일한 패턴 적용. `registerDefaultHandlers()` 도입.
7. **CheckerTest fixture 정리** - 테스트마다 반복되던 준비 코드를 `TEST_F` fixture로
   이동.
8. **이번 라운드(ImplementTodo.md §3 Checker 담당자 항목)**:
   - `checkImport` 스코프 누락 수정 - import 내부 선언이 바깥 스코프로 새는 버그
     (PR #36 지적사항)를 `checkBlock`과 같은 패턴으로 수정.
   - 상속 의미 검사 추가 - 자기 자신 상속 금지, 클래스가 아닌 대상 상속 금지,
     `Super`의 스코프/부모 클래스 여부 검사(`classNames_`, `hasSuperclass_` 도입).
   - Checker/Optimizer 책임 분리 - `foldConstantIfPossible`을 `Optimizer`로 이전하고,
     완화된 non-const 필드를 활용해 **실제 트리 치환**을 완성. 상수 폴딩 테스트를
     `OptimizerTest.cpp`로 이동.
