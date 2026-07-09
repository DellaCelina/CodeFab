# Executor

`Checker`를 통과한 `SyntaxTree`를 실제로 실행하는 Unit입니다. Statement/Expression
노드를 DFS로 순회하며 부수효과(변수 대입, 출력 등)를 일으키고, Expression은 `Value`를
계산해 반환합니다.

## 책임

- `SyntaxTree`를 받아 프로그램을 처음부터 끝까지 실행한다 (`execute(SyntaxTree&)`).
- 표현식 하나만 평가해서 값을 반환한다 (`evaluate(Expression*)`) - `RunPromptShell`의
  정상 파이프라인에서는 쓰이지 않지만, `Checker`의 상수 폴딩이 산술 규칙을 다시
  구현하지 않고 이 메서드를 재사용한다.
- 변수 저장소(`Environment`/`Scope`)를 운용하며 블록 스코프, 함수/메서드 호출, 클래스
  인스턴스, 배열, 모듈(import)의 런타임 표현을 관리한다.
- 실행 중 타입 불일치, 미정의 변수, 0으로 나누기 등의 런타임 오류를 `ExecutorError`로
  던진다.

## 파일 구성

| 파일 | 역할 |
|---|---|
| `ExecuteInterface.h` | `ExecuteInterface` 추상 클래스 + `ExecutorError` 정의 |
| `Executor.h` / `.cpp` | `ExecuteInterface` 구현체. `type_index -> handler` 테이블로 Statement/Expression을 디스패치하는 DFS 실행기 |
| `Environment.h` / `.cpp` | `Scope` 스택 관리. `pushScope`/`popScope`, 동적 조회(`lookup`/`assign`)와 정적 바인딩 조회(`lookupAt`/`assignAt`) 제공 |
| `Scope.h` / `.cpp` | 블록 스코프 하나(이름 → `Value` 테이블). 클래스 인스턴스 필드, 모듈 export 스코프로도 재사용됨 |
| `Value.h` / `.cpp` | 런타임 값 타입(`Nil`/`Boolean`/`Number`/`String`/`Function`/`Class`/`Instance`/`Array`/`Module`) |
| `InstanceValue.h` | 클래스 인스턴스 하나(`klass` 비소유 포인터 + `fields` 공유 `Scope`) |
| `ArrayValue.h` | 고정 크기 배열 하나(`std::vector<Value>`) |

## 핵심 설계

- **핸들러 테이블 디스패치**: `Executor`는 `dynamic_cast`/`switch` 체인 대신
  `std::type_index -> std::function` 테이블(`statementHandlers_`/`expressionHandlers_`)로
  노드 타입을 처리한다. 새 노드 종류를 추가할 때 `execute()`/`evaluate()` 자체를 고칠
  필요 없이 `registerDefaultHandlers()`만 늘어난다.
- **AST 노드를 값으로 재사용**: 함수/클래스는 별도의 `FunctionObject`/`ClassObject`
  래퍼 없이 `FunctionDeclareStatement*`/`ClassDeclareStatement*`를 `Value`가 직접
  들고 있다. 선언 노드는 `SyntaxTree`가 프로그램 실행 내내 소유하므로 비소유 참조로도
  안전하다.
- **`Scope` 재사용**: 클래스 인스턴스의 필드 저장소(`InstanceValue::fields`), import한
  모듈의 export 스코프가 모두 블록 스코프와 같은 `Scope` 타입을 그대로 쓴다. "없는
  이름에 대입하면 새로 생성"이라는 `Scope::define`의 기존 동작이 "동적 필드 추가"
  요구사항과 정확히 일치하기 때문에 별도 자료구조가 필요 없다.
- **정적 바인딩(`lookupAt`/`assignAt`)**: `Checker`의 Resolver가 `IdentifierExpression`에
  채워 넣은 `depth`를 그대로 받아, 매번 전체 스코프를 훑는 `lookup`/`assign`보다 빠르게
  정확히 그 스코프 하나만 조회한다.

## 테스트가 다루는 범위

| 파일 | 커버 범위 |
|---|---|
| `EnvironmentTest.cpp` | 스코프 push/pop, define/lookup/assign, 바깥 스코프로의 대입 전파 |
| `ExecutorTest.cpp` | 산술/비교/논리(`and`/`or`, short-circuit 포함)/`%` 연산자 평가, 타입 오류·0으로 나누기 등 평가 단계 런타임 오류 |
| `ExecutorVariableTest.cpp` | 변수 선언/재대입, 블록 스코프 쉐도잉, 중첩 블록에서의 스코프 해석 |
| `ExecutorControlFlowTest.cpp` | `if`/`else`(댕글링 else 포함), `for`(변수 초기화, 조건 평가) |
| `ExecutorFunctionTest.cpp` | 함수 선언/호출/재귀, 인자 개수 불일치·호출 불가능한 값 등 오류, 매개변수 스코프 격리 |
| `ExecutorClassTest.cpp` | `init`/필드 read·write, 존재하지 않는 필드·메서드 접근 오류, `This` 오용, `instanceof`, 동적 필드 추가 |
| `ExecutorArrayTest.cpp` | 배열 생성 및 기본값, 인덱스 read/write, 크기·인덱스 타입 오류, 범위 초과 오류 |
| `ExecutorImportTest.cpp` | import로 만든 alias를 통한 변수/함수 접근 |
