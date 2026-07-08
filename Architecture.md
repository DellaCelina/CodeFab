# CodeFab Interpreter — 3일차 기능 확장 아키텍처

> 이 문서는 1일차 구현(현재 코드) 기준에서 3일차 요구사항(`3일차_CodeFab Interpreter.pdf`)의
> function / class / 정적 배열 / 실행전 최적화 / import / 공장 제어 쉘 을 지원하기 위해
> 필요한 아키텍처 변경을 설계한다. **이 문서는 설계 문서이며, 실제 `.h`/`.cpp` 코드는 아직
> 수정하지 않았다.** 구현 시 이 문서를 기준으로 각 Unit을 확장한다.

## 0. 목표와 범위

- 기존 4-Unit 파이프라인(Tokenizer → Assembler → Checker → Executor)과 Shell의 책임 분리
  원칙을 그대로 유지한 채, 새 기능을 어느 Unit에 어떤 형태로 얹을지 정한다.
- 가능한 한 **기존 클래스의 시그니처를 유지**하고, 확장이 꼭 필요한 지점만 최소한으로
  변경한다 (예: `Value`의 저장 가능 타입 확장, `SyntaxTree`에 메타데이터 필드 추가).
- 새로 등장하는 교차 관심사(정적 바인딩, 상수 폴딩, import 해석)는 기존 Unit의 책임을
  침범하지 않도록 별도 하위 컴포넌트 또는 새 Unit으로 분리한다.

## 1. 파이프라인 변경 개요

```
소스코드(string)
      │
      ▼
┌────────────┐  Token   ┌────────────┐ SyntaxTree ┌──────────────────────┐  최적화된    ┌────────────┐
│ Tokenizer  │─────────▶│ Assembler  │───────────▶│ Checker              │  SyntaxTree  │ Executor   │
└────────────┘          └────────────┘             │  1) 의미 오류 검사     │─────────────▶└────────────┘
      ▲                                             │  2) Resolver(정적바인딩)│                  │
      │                        ┌───────────────────▶│  3) ConstantFolder    │                  │
      │                        │                     └──────────────────────┘                  │
      │                        │                              ▲                                 │
      │                 ┌──────┴──────┐                       │ import 시 재귀 컴파일             │
      │                 │   Import    │◀──────────────────────┘                                 │
      │                 │ (신규 Unit)  │──────────────────────────────────────────────────────────┘
      │                 └─────────────┘            import 모듈 실행/캐시
      │
      └── 공장 제어 쉘 (Shell) : REPL mode / File mode / Debug mode 가 위 파이프라인을 감싼다
```

새 Unit: **Import** 하나만 추가한다. function/class/배열/최적화는 기존 4-Unit 내부 확장으로
처리한다(각각 새 Unit을 만들 만큼 독립적인 파이프라인 단계가 아니라, 기존 단계의 문법/의미/실행
규칙이 늘어나는 것이기 때문).

## 2. 공통 기반 확장

### 2.1 Token / TokenType 추가

`Tokenizer/Token.h`의 `TokenType`에 아래를 추가한다.

| 분류 | 추가 Token |
|---|---|
| 키워드 | `FUNC`, `RETURN`, `CLASS`, `THIS`, `SUPER`, `INSTANCEOF`, `IMPORT`, `ALIAS` |
| 구분자 | `LEFT_BRACKET` (`[`), `RIGHT_BRACKET` (`]`), `COMMA` (`,`), `DOT` (`.`), `COLON` (`:`) |

`COMMA`/`DOT`는 함수 인자 목록·필드 접근에, `COLON`은 `Class B : A` 상속 선언에,
`LEFT_BRACKET`/`RIGHT_BRACKET`은 배열 인덱싱에 필요하다.

### 2.2 SyntaxTree 노드 확장과 불변성 완화

`Assembler/SyntaxTree.h`에 아래 노드를 추가한다. 기존 노드와 동일하게
`SyntaxNode(tokens)` 생성자 패턴, `operator==` 오버라이드 관례를 따른다.

**Expression 계열**

- `CallExpression(callee, arguments)` — 함수 호출과 클래스 인스턴스 생성(`Robot()`)에
  동일하게 사용한다. 클래스 인스턴스 생성은 "클래스 이름을 callee로 하는 호출"로 통일해서
  모델링한다(문법적으로 함수 호출과 구분할 필요가 없어짐).
- `GetExpression(object, name)` — `r.name`, `r.speed` 필드/메서드 읽기.
- `SetExpression(object, name, value)` — `r.speed = 10` 필드 쓰기.
- `ThisExpression(keyword)`
- `SuperExpression(keyword, method)`
- `InstanceOfExpression(object, className)`
- `IndexGetExpression(array, index)` — `arr[i]` 읽기.
- `IndexSetExpression(array, index, value)` — `arr[i] = v` 쓰기.

**Statement 계열**

- `FunctionDeclareStatement(name, params, body)` — `Func add(a, b) { ... }`.
- `ReturnStatement(keyword, value?)` — `value`는 없을 수 있음(nullptr 허용).
- `ClassDeclareStatement(name, superclass?, methods)` — `superclass`는
  `IdentifierExpression*`(nullable), `methods`는 `FunctionDeclareStatement*` 목록.
- `ImportStatement(path, alias)` — `path`는 문자열 리터럴 Token, `alias`는 식별자 Token.

**불변성 완화(중요한 설계 결정)**

기존 노드는 자식을 `Expression* const`/`Statement* const`로 선언해 생성 후 재배선이
불가능하다. §2.6의 상수 폴딩(Constant Folding)은 이미 만들어진 `BinaryExpression`의
서브트리를 리터럴 노드로 **치환**해야 하므로, 치환 대상이 될 수 있는 필드
(`BinaryExpression::left/right`, `UnaryExpression::operand`, 각 Statement가 담고 있는
`Expression*` 필드들)는 `const`를 제거하고 일반 포인터로 완화한다. 다른 필드(`identifier`,
`name` Token 등 구조를 바꾸지 않는 필드)는 기존처럼 `const`를 유지해도 무방하다.

### 2.3 SyntaxTree 부가 메타데이터 — 변수 해석 거리(Resolutions)

정적 바인딩 최적화(§2.6)를 위해 Checker가 계산한 "변수 이름 → 몇 단계 상위 스코프인지"
정보를 어딘가에 저장해서 Executor에 전달해야 한다. AST 노드 자체에 필드를 추가하는 대신,
**`SyntaxTree`에 사이드 테이블을 둔다.**

```cpp
// Assembler/SyntaxTree.h
class SyntaxTree {
public:
    ...
    void resolve(const SyntaxNode* node, int distance);
    std::optional<int> resolution(const SyntaxNode* node) const;
private:
    std::unordered_map<const SyntaxNode*, int> resolutions_;
};
```

- `IdentifierExpression`(변수 참조)과 `AssignExpression`(대입 대상)에 대해 Checker가
  `distance`(0 = 현재 스코프, 1 = 바로 위 스코프, …)를 계산해 `tree.resolve(node, distance)`로
  기록한다.
- 전역 변수이거나(모든 로컬 스코프에서 못 찾음) `import`로 들여온 모듈처럼 정적으로 거리를
  확정할 수 없는 경우 `resolve()`를 호출하지 않고, Executor는 `resolution()`이 `nullopt`이면
  기존처럼 동적 조회(global 우선/문자열 조회)로 폴백한다.
- 노드 자체는 불변으로 유지되고(§2.2에서 완화한 자식 포인터 필드는 트리 구조 변경용이지,
  해석 결과 저장용이 아님), Checker → Executor 사이의 계약이 `SyntaxTree`라는 이미 존재하는
  통로로 자연스럽게 흐른다.

### 2.4 Value 확장 — 함수/클래스/인스턴스/배열/모듈

`Executor/Value.h`의 `Type` enum과 내부 `std::variant`에 참조 타입을 추가한다. 인스턴스
필드 변경이 모든 참조자에게 보여야 하므로(참조 의미론) 아래는 전부 `shared_ptr`로 감싼다.

```cpp
enum class Type { Nil, Boolean, Number, String, Function, Class, Instance, Array, Module };

std::variant<std::monostate, bool, double, std::string,
             std::shared_ptr<FunctionObject>,
             std::shared_ptr<ClassObject>,
             std::shared_ptr<InstanceObject>,
             std::shared_ptr<ArrayObject>,
             std::shared_ptr<ModuleObject>> data_;
```

새 런타임 객체는 `Executor/` 폴더에 각각 헤더로 추가한다(런타임 상태이므로 `Value`,
`Environment`, `Scope`와 같은 위치).

- **`FunctionObject`** : `FunctionDeclareStatement* declaration`, `std::shared_ptr<Environment> closure`,
  (메서드인 경우) `std::optional<Value> boundThis`. `bind(Value instance)`로 메서드를
  인스턴스에 바인딩한 새 `FunctionObject`를 만든다(`this`가 자유 변수로 캡처된 클로저).
- **`ClassObject`** : `std::string name`, `std::shared_ptr<ClassObject> superclass`(nullable),
  `std::unordered_map<std::string, std::shared_ptr<FunctionObject>> methods`.
  `findMethod(name)`는 자기 메서드에 없으면 `superclass`로 재귀 위임한다.
- **`InstanceObject`** : `std::shared_ptr<ClassObject> klass`,
  `std::unordered_map<std::string, Value> fields`. `get(name)`은 필드 우선, 없으면
  클래스 메서드를 찾아 `bind(this)`해서 반환, 둘 다 없으면 `ExecutorError`.
  `isInstanceOf(klass)`는 `klass` 체인을 따라 올라가며 비교(§4.5 instanceof).
- **`ArrayObject`** : `std::vector<Value> items`(고정 크기, 생성 시 `null`로 채움).
- **`ModuleObject`** : `import`로 들여온 파일의 최상위 전역 스코프 스냅샷
  (`std::unordered_map<std::string, Value>`, 함수/전역변수 이름 → 값). §7 참고.

`Environment`/`Scope`는 값 타입을 그대로 `Value`로 다루므로 변경이 필요 없다(단, §2.6에서
`Environment`에 거리 기반 접근 메서드를 추가한다).

## 3. Function 지원 설계

### 3.1 Assembler

- `parseDeclaration()`에 `FUNC` 분기 추가 → `parseFunction()`이
  `FunctionDeclareStatement(name, params, body)`를 만든다. `body`는 기존 블록 파싱 재사용.
- Primary/Call 파싱 레벨에 `CallExpression` 생성 로직 추가: 1차 표현식을 파싱한 뒤
  `(`가 연속되면 계속 `CallExpression(callee=이전 결과, arguments)`로 감싸 좌결합 호출
  체인(`f()()`, 나중에 `r.move(5)`와 결합)을 지원한다.
- `return` 문 파싱: `RETURN` 토큰 소비 후 `;`가 바로 오면 `value=nullptr`, 아니면
  표현식 파싱.

### 3.2 Checker

- 스코프 스택에 "현재 함수 안인지" 플래그(중첩 함수 고려 시 카운터)를 추가해서
  `ReturnStatement`가 함수 바깥에서 나타나면 `CheckerError`.
- `FunctionDeclareStatement` 검사 시 파라미터 이름 집합에 중복이 있으면 `CheckerError`.
- `CallExpression` 검사: 정적으로 인자 개수를 확정할 수 있는 경우(callee가 같은 스코프에서
  찾은 `FunctionDeclareStatement`를 가리키는 이름일 때) 파라미터 개수와 인자 개수를 비교해
  `CheckerError`. **호출 대상이 함수인지 자체는 런타임에만 알 수 있으므로**("함수가 아닌
  대상 호출"), 이 검사는 Checker가 아니라 Executor의 런타임 오류로 둔다(§3.3).

### 3.3 Executor

- `CallExpression` 핸들러: callee를 평가해 `Value`가 `Function`/`Class`가 아니면
  `ExecutorError`("호출할 수 없는 대상입니다"). `Function`이면 인자 개수를 파라미터 개수와
  다시 한 번 런타임에서도 대조(재귀/모듈 함수처럼 Checker가 정적으로 못 본 경우 대비).
- 함수 호출 시 `Environment`에 `closure` 기준으로 새 스코프를 push 하고 파라미터를
  bind, `body`를 실행한다. **return 처리**: `ReturnStatement` 실행 시 C++ 예외
  `Executor::ReturnSignal{ Value value }`(공개 API가 아닌 내부 전용 타입, `ExecutorError`처럼
  `std::exception`을 상속하지 않는 순수 제어 흐름 신호)를 던지고, 함수 호출 핸들러가
  `catch (ReturnSignal&)`로 받아 그 값을 `CallExpression`의 평가 결과로 사용한다. `return`이
  없으면 함수 종료 시 `Nil`.
- 재귀 호출은 매 호출마다 새 `Environment` 프레임을 만드는 것으로 자연히 지원된다(closure
  체인은 선언 시점 환경을 고정 캡처하므로 재귀 참조도 문제없다 — 단, 함수 이름 자체는
  선언 시점에 자신의 정의 스코프에 먼저 등록한 뒤 body를 파싱/바인딩해야 자기 참조가
  가능함에 유의).

## 4. Class 지원 설계

### 4.1 Assembler

- `parseDeclaration()`에 `CLASS` 분기 추가. `Class Name (: Superclass)? { method* }`를
  파싱해 `ClassDeclareStatement`를 만든다. 메서드는 `Func` 키워드 없이 `이름(params) { ... }`
  형태이므로, 클래스 바디 안에서는 함수 선언 파싱기를 "이름으로 시작하면 메서드"로
  재사용한다(`FunctionDeclareStatement`를 그대로 메서드 표현에도 사용).
- Primary 표현식에 `THIS`, `SUPER` 분기 추가: `This` → `ThisExpression`,
  `Super.method(...)` → `.` 다음 식별자를 읽어 `SuperExpression` 생성 후 바로 이어지는
  `CallExpression`으로 감쌈(문법상 `Super.x`만 단독으로 등장하는 것은 허용하지 않고 항상
  호출/메서드 참조 형태로 강제할지는 팀 컨벤션으로 결정. 슬라이드 예시는 항상 호출 형태).
- Call 파싱 체인에 `.`을 추가: `expr ( "." IDENTIFIER | "(" args ")" | "[" expr "]" )*`
  구조로 확장해서 `r.name`, `r.move(5)`, `arr[i]`를 동일 루프에서 좌결합으로 파싱한다.
  `.` 다음에 `=`이 오면 `GetExpression` 대신 `SetExpression`으로 재작성(기존
  `AssignExpression` 파싱이 대입 좌변을 사후 검증하는 방식과 동일한 패턴).
- `instanceof`는 이항 연산자처럼 파싱하되(우선순위는 비교 연산자와 동급 정도),
  `InstanceOfExpression(left, right-as-IdentifierExpression)`을 생성. 우변이 식별자가
  아니면 파싱 오류.

### 4.2 Checker

- 클래스 선언 검사: `Class A : A`(자기 상속) → `CheckerError`. `Class A : x`에서 `x`가
  같은 스코프의 클래스 선언이 아니면 `CheckerError`("클래스가 아닌 대상 상속") — 이는
  정적으로 판별 가능(전역/블록 스코프에 등록된 이름이 `ClassDeclareStatement`인지 확인).
- `This`/`Super` 사용 위치 검사: 함수와 유사하게 "현재 클래스 메서드 안인지" 스택을 두고,
  클래스 바깥에서 `This`/`Super` 사용 시 `CheckerError`. `Super` 사용 시 현재 클래스에
  `superclass`가 없으면 `CheckerError`.
- `init` 메서드에서 `return <expr>;`(값 있는 return) 사용 시 `CheckerError`. `return;`
  (빈 return, 조기 종료)은 허용할지 여부는 팀 컨벤션(슬라이드는 "return 허용 x"이므로
  빈 return도 금지하는 것을 기본값으로 한다).
- "인스턴스가 아닌 대상의 필드 접근"(`var x = "hello"; x.field = 1;`)과 "존재하지 않는
  필드/메서드 접근"은 런타임에만 값의 실제 타입/필드 존재 여부를 알 수 있으므로 Executor의
  런타임 오류로 둔다.

### 4.3 Executor — 인스턴스/필드/메서드

- `CallExpression`의 callee가 `Value::Type::Class`이면: 새 `InstanceObject`를 만들고,
  `klass.findMethod("init")`가 있으면 `init`을 `this=새 인스턴스`로 바인딩해 호출(인자 전달),
  결과는 버리고 항상 새 인스턴스를 반환한다(“init은 항상 인스턴스 반환”).
- `GetExpression`: object를 평가해 `Instance`가 아니면 `ExecutorError`. 필드 맵에 있으면
  그 값, 없으면 `klass.findMethod(name)`으로 찾아 `bind(instance)` 후 `Function` Value로
  반환, 둘 다 없으면 `ExecutorError`("존재하지 않는 필드/메서드").
- `SetExpression`: object가 `Instance`가 아니면 `ExecutorError`. 있으면 필드 맵에
  `[name] = value`(없던 필드면 새로 생성).
- `ThisExpression`/`SuperExpression`은 **변수처럼** 취급한다: 메서드 호출 시 함수 호출과
  동일하게 새 스코프를 열면서 그 스코프에 `"this"`라는 이름으로 바인딩된 인스턴스 값을
  `define`한다. `Super.method(...)` 평가 시에는 현재 스코프에서 `"this"`를 찾고,
  메서드가 정의된 클래스의 `superclass`에서 `findMethod`를 시작해 바인딩한다(오버라이드를
  건너뛰고 부모 구현을 호출).
  - `§2.3`의 정적 바인딩과 결합: `this`/`super`도 일반 지역 변수처럼 Resolver가 distance를
    계산해 O(1)로 찾을 수 있다(암시적으로 매 메서드 호출 스코프의 최상단에 정의되므로
    Resolver 관점에서는 `var this = ...;`가 있는 것과 동일하게 취급).
- `InstanceOfExpression`: 좌변을 평가해 `Instance`가 아니면 `false`(또는 타입 오류로
  할지는 팀 결정, 문서는 `false` 권장 — 다른 언어의 일반적 관례). 우변 클래스 이름으로
  전역에서 `ClassObject`를 조회하고, `instance.klass`부터 `superclass` 체인을 따라
  올라가며 동일 객체인지 비교.

## 5. 정적 배열 설계

### 5.1 Assembler

- `Array` 는 **내장 함수처럼 호출되는 특수 식별자**가 아니라, Executor가 전역 환경에
  미리 등록해두는 **내장 콜러블 값**으로 처리한다(즉 Assembler/Checker 입장에서는
  `Array(3)`도 그냥 평범한 `CallExpression`이다 — 새 문법 규칙이 필요 없다). 이렇게 하면
  "함수처럼 호출해서 생성"이라는 슬라이드의 클래스 인스턴스 생성 방식과 대칭적이다.
- 인덱스 표현식만 새 문법으로 추가: Call 파싱 체인(§4.1에서 이미 `.`/`(`와 함께 확장한
  체인)에 `"[" expression "]"`을 추가해 `IndexGetExpression`을 만들고, 대입 좌변으로
  쓰이면 `IndexSetExpression`으로 재작성.

### 5.2 Executor

- 전역 `Environment`(프로그램 시작 시 1회) 초기화 단계에서 `"Array"`라는 이름에
  내장 `FunctionObject`(네이티브 함수, `declaration` 대신 C++ 람다를 실행하는 변형)를
  등록한다. 이를 위해 `FunctionObject`에 "선언 기반" 또는 "네이티브 콜백 기반" 두 종류를
  두거나(`std::variant<FunctionDeclareStatement*, NativeFn>`), 별도의 경량
  `NativeFunctionObject`를 만들고 `Value::Type::Function`이 이 둘 중 하나를 가리키도록
  한다. 네이티브 `Array` 콜백은: 인자가 1개, `Number`가 아니면 `ExecutorError`("배열의
  사이즈는 반드시 number"), 정수로 변환해 크기만큼 `Nil`로 채운 `ArrayObject`를 만들어
  `Array` 타입 `Value`로 반환.
- `IndexGetExpression`/`IndexSetExpression` 핸들러: 대상 평가 결과가 `Array`가 아니면
  `ExecutorError`("index 접근은 오직 배열만 지원"). 인덱스 평가 결과가 `Number`가 아니면
  `ExecutorError`. 인덱스가 `[0, size)` 범위를 벗어나면 `ExecutorError`("인덱스 범위
  벗어남"). 그 외에는 `items[index]` 읽기/쓰기.

## 6. 실행 전 최적화 설계

두 최적화 모두 **Checker Unit 내부의 후처리 하위 컴포넌트**로 구현해서, `Checker::check()`가
"의미 오류 검사"에 이어 "성공 시에만" 수행하도록 한다(오류가 있는 트리를 최적화할 필요는
없음).

```
Checker::check(SyntaxTree& tree)
   1) SemanticAnalyzer  — 기존 의미 오류 검사(중복 선언/자기참조/§3~4의 새 검사들)
   2) Resolver          — 통과 시: 변수/this/super 참조에 대해 distance 계산 후 tree.resolve()
   3) ConstantFolder    — 통과 시: 트리를 순회하며 순수 상수 이항연산 서브트리를 리터럴로 치환
```

세 하위 컴포넌트는 `Checker.cpp` 내부의 private 클래스/함수로 두거나(작은 프로젝트 규모를
고려하면 이 편을 권장), 파일이 커지면 `Checker/Resolver.h/.cpp`, `Checker/ConstantFolder.h/.cpp`로
분리한다. 어느 쪽이든 `CheckerInterface`의 공개 시그니처(`bool check(SyntaxTree&)`)는
바뀌지 않는다.

### 6.1 Resolver (정적 바인딩)

- 기존 `SemanticAnalyzer`가 이미 "중복 선언"을 검사하려고 스코프 스택(블록마다 선언된
  이름 집합)을 관리하고 있으므로, 그 순회에 편승해서 각 스코프에 "선언 순서" 대신 이번엔
  "몇 번째 블록인지" 깊이만 추적하면 된다.
- `IdentifierExpression`(변수 참조), `AssignExpression`의 대상, `ThisExpression`,
  `SuperExpression`을 만날 때마다: 현재 스코프부터 바깥쪽으로 몇 단계를 올라가야 그
  이름이 선언된 스코프가 나오는지 센다. 찾으면 `tree.resolve(node, distance)`. 어느
  로컬 스코프에서도 못 찾으면(전역이거나 import 모듈 이름) 아무것도 기록하지 않는다
  (Executor가 기존 방식대로 전역까지 훑는 동적 조회로 폴백).
- `Environment`에 거리 기반 접근 메서드를 추가한다.

```cpp
// Executor/Environment.h
void defineAt(int distance, const std::string& name, const Value& value); // 필요시
std::optional<Value> lookupAt(int distance, const std::string& name) const;
bool assignAt(int distance, const std::string& name, const Value& value);
```

  `distance`만큼 `scopes_`를 안쪽에서부터 건너뛰어 정확히 그 `Scope`에서만 조회하므로
  O(depth 무관, O(1) 수준의 상수 시간 인덱싱)이다. Executor의 `evaluate(IdentifierExpression*)`,
  `execute(AssignExpression*)` 핸들러는 `tree.resolution(node)`가 있으면 `*At` 계열을,
  없으면 기존 `lookup`/`assign`(동적 조회)을 호출하도록 분기한다. **Executor가
  `SyntaxTree&`(또는 최소한 resolution 조회 인터페이스)에 접근할 수 있어야 하므로,
  `ExecuteInterface::execute(SyntaxTree&)`는 이미 트리 전체를 받고 있어 문제없지만, 내부의
  `execute(Statement*)`/`evaluate(Expression*)` private 메서드들도 현재 실행 중인
  `SyntaxTree`에 대한 참조를 갖고 있어야 한다(생성자 대신 `execute(SyntaxTree&)` 호출 시
  멤버로 잠깐 보관하거나, 각 핸들러에 tree 참조를 함께 넘기도록 시그니처를 확장).**

### 6.2 ConstantFolder (상수 연산 최적화)

- Assembler가 만든 트리를 **bottom-up으로 재귀 순회**하며, `BinaryExpression`(및 하위
  `AddExpression`/`SubExpression`/`MultExpression`/`DivideExpression`/비교 연산 등)의
  `left`/`right`가 모두 이미 리터럴(`NumberExpression`/`BooleanExpression`/`StringExpression`)
  이면 즉시 상수 연산을 수행해 하나의 새 리터럴 노드로 접는다.
- §2.2에서 자식 포인터 필드를 non-const로 완화했으므로, 폴더는 **제자리 치환**을 수행한다:
  자식을 먼저 재귀적으로 접은 뒤(포스트오더), 부모가 여전히 `BinaryExpression`이고 두
  자식이 모두 리터럴이면 새 `NumberExpression`(또는 해당 타입 리터럴)을 만들어
  `tree.add(std::make_unique<NumberExpression>(...))`로 소유권을 트리에 등록하고, **부모의
  `left`/`right` 필드를 새 노드로 덮어쓴다.** 원래 있던 `BinaryExpression`과 그 자식들은
  더 이상 참조되지 않지만 `SyntaxTree::nodes_`가 계속 소유하고 있으므로 메모리 안전성은
  깨지지 않는다(단순히 안 쓰이는 노드로 남을 뿐 — 트리 규모상 문제없는 트레이드오프).
- 나눗셈(`/`)처럼 0으로 나누는 경우 등 폴딩 시점에 런타임 오류가 나는 상황은 **폴딩하지
  않고 원래 트리를 그대로 둔다**(슬라이드의 "100% 확정 가능할 때만 최적화"라는 조건과
  일치 — 0-나눗셈은 여전히 Executor가 런타임에 검출해야 하는 오류이므로 미리 접으면
  오류 발생 시점/줄 번호가 달라져 버그가 된다).
- 폴딩 대상은 순수 산술/비교 연산으로 한정하고, 부작용이 있을 수 있는 표현식(함수 호출,
  대입, `this`/`super`, 변수 참조 등)은 절대 대상에 포함하지 않는다(리터럴 여부만 보므로
  자동으로 배제됨).

### 6.3 테스트 전략 (Test Double)

슬라이드가 요구한 대로 두 최적화는 **Test Double로 검증**한다.

- **Resolver 검증**: 페이크 `Environment`(또는 진짜 `Environment` + 접근 카운터 래퍼)를
  준비해 `lookupAt`/`assignAt` 호출 횟수와 `lookup`/`assign`(동적 조회) 호출 횟수를
  세는 스파이(spy)를 만든다. 중첩 블록 안에서 지역 변수를 사용하는 스크립트를 실행했을
  때, 동적 조회가 전혀 발생하지 않고 전부 `*At` 경로로 갔는지 단언한다.
- **ConstantFolder 검증**: `Value::asNumber()` 호출 횟수를 세는 계측용 `Executor` 서브클래스
  또는 evaluate 호출 카운터를 두고, 루프 안에 상수식이 있는 스크립트를 최적화 전/후로
  실행해 "연산 횟수가 N회 → 0회"로 줄었는지 확인한다. 트리 구조 검증(폴딩 후
  `BinaryExpression` 노드가 사라지고 `NumberExpression` 하나만 남았는지)도 `AssemblerTest`류
  테스트처럼 트리를 직접 비교(`operator==`)해서 확인할 수 있다.

## 7. Import 설계

### 7.1 새 Unit: Import

기존 4-Unit 어디에도 깔끔히 속하지 않는 책임(파일을 읽고, 그 내용을 다시 전체
파이프라인에 태워 컴파일하고, 결과를 캐싱하고, 순환을 검출하는 일)이므로 **새 Unit
폴더 `Import/`를 추가**한다. 다른 Unit과 동일한 패턴(인터페이스 + 구현 + 전용 예외)을
따른다.

```
Import/
├── ImportResolverInterface.h   ImportResolverInterface 추상 클래스 + ImportError
├── SourceLoaderInterface.h     파일 I/O를 추상화 (테스트에서 Fake로 대체하기 위함)
├── ImportResolver.h / .cpp     ImportResolverInterface 구현체
└── ImportResolverTest.cpp
```

```cpp
// Import/ImportResolverInterface.h
struct ImportResolverInterface {
    virtual ~ImportResolverInterface() = default;
    // path 의 소스를 로드해 Tokenizer+Assembler+Checker(재귀적으로 그 파일의 import까지)
    // 로 컴파일한 뒤, 실행하여 얻은 전역 선언들을 ModuleObject 로 반환한다.
    // 이미 같은 path 를 컴파일/실행한 적이 있으면 캐시된 결과를 재사용한다.
    virtual std::shared_ptr<ModuleObject> resolve(const std::string& path) = 0;
};
```

`ImportResolver`는 생성자에서 `TokenizeInterface&`, `AssemblerInterface&`,
`CheckerInterface&`, `ExecuteInterface&`, `SourceLoaderInterface&`를 주입받는다(4-Unit을
재귀적으로 재사용 — RunPromptShell과 형제 관계인 또 하나의 파이프라인 조립자다). 내부적으로
`std::vector<std::string> resolving_`(현재 해석 중인 경로 스택, 순환 검출용)와
`std::unordered_map<std::string, std::shared_ptr<ModuleObject>> cache_`를 갖는다.

### 7.2 Assembler / Checker

- `ImportStatement` 파싱: `import STRING alias IDENTIFIER ;`.
- Checker는 **파일을 열지 않고** 아래를 정적으로 검사한다:
  - 반복문(`ForStatement`) 바디 내부에서 `ImportStatement`를 만나면 `CheckerError`.
  - 같은 스코프 내 동일 `path` 중복 import → `CheckerError`.
  - 상위 스코프에서 이미 import된 `path`를 하위 스코프에서 다시 import → `CheckerError`
    (스코프 스택을 따라 올라가며 검사).
  - alias 이름이 같은 스코프의 다른 선언과 충돌하면 일반 변수 중복 선언과 동일한 규칙
    적용.
- **순환 import**와 **대상 파일 없음**은 파일을 실제로 열어야 알 수 있으므로, Checker가
  `ImportResolverInterface&`를 주입받아 `ImportStatement`를 만날 때마다
  `resolver.resolve(path)`를 호출한다(성공하면 이후 alias를 "존재하는 모듈"로 현재 스코프에
  등록해 `sum.add`같은 멤버 접근을 §4.2/4.3의 `GetExpression` 규칙으로 자연스럽게 검사할 수
  있게 한다). `ImportResolver`가 순환을 감지하면 `ImportError`를 던지고, Checker는 이를
  잡아 `CheckerError`로 변환(또는 `CheckerInterface`가 `ImportError`도 그대로 던지도록
  허용 — 기존 `RunPromptShell`이 `AssemblyError`/`CheckerError`/`ExecutorError`를 각각
  잡던 곳에 `ImportError`도 추가로 잡도록 확장).

### 7.3 Executor

- `ImportStatement` 실행: `resolver.resolve(path)`(Checker 단계에서 이미 캐시되어 있으므로
  실질적으로 재계산 없이 캐시 히트) 결과인 `ModuleObject`를 alias 이름으로 현재
  `Environment`에 `define`한다(`Value::Type::Module`).
- `GetExpression`의 object가 `Module`이면 `ModuleObject`의 내보낸 이름 테이블에서 조회해
  `Function`/그 외 값을 반환한다(인스턴스 필드 조회와 동일한 코드 경로를 태우기 위해
  `InstanceObject`/`ModuleObject`를 "멤버 접근 가능한 객체"라는 공통 개념으로 다루는
  작은 헬퍼 인터페이스(`MemberAccessible`)를 둬도 좋다 — 필수는 아니고 중복 코드가
  거슬리면 도입).

### 7.4 순환 참조 없는 계층

`Import`는 Tokenizer/Assembler/Checker/Executor 네 인터페이스에만 의존하고, 그 반대로
Checker/Executor가 `Import`에 의존하는 방향이 새로 생긴다. 이는 "Tokenizer→Assembler→
{Checker, Executor}→Shell" 이라는 기존 단방향 계층을 깨지 않으면서, `Import`를 Shell과
동급의 "조립자" 계층에 두고 Checker/Executor가 `ImportResolverInterface`라는 얇은 추상에만
의존하게 함으로써(구체 클래스가 아니라 인터페이스 의존) 순환을 피한다. `main.cpp`가
`ImportResolver` 인스턴스 하나를 만들어 Checker와 Executor 양쪽에 주입한다(캐시를
공유해야 하므로 반드시 같은 인스턴스여야 함).

## 8. 공장 제어 쉘 설계

### 8.1 모드 분리

`Shell/main.cpp`가 CLI 인자를 파싱해 세 모드 중 하나로 위임하는 구조로 바뀐다.

```
Shell/
├── RunPromptShell.h/.cpp        기존 REPL 모드 (변경 없음, 그대로 재사용)
├── FileRunMode.h/.cpp           신규: 파일 모드
├── DebugMode.h/.cpp             신규: 디버그 모드
├── Debugger.h/.cpp              신규: 디버그 모드의 명령 처리기(step/break/watch 등)
├── CommandLineArgs.h/.cpp       신규: argv 파싱 (테스트하기 쉽게 main.cpp에서 분리)
└── main.cpp                     CommandLineArgs 로 모드를 고른 뒤 위임
```

```cpp
// main.cpp (개념)
auto args = CommandLineArgs::parse(argc, argv);
switch (args.mode) {
  case Mode::Repl:  RunPromptShell(tokenizer, assembler, checker, executor).run(cin, cout); break;
  case Mode::Run:   FileRunMode(tokenizer, assembler, checker, executor).run(args.path, cout); break;
  case Mode::Debug: DebugMode(tokenizer, assembler, checker, executor).run(args.path, cin, cout); break;
}
```

`FileRunMode`/`DebugMode`도 `RunPromptShell`과 마찬가지로 4개 `*Interface`(및 Import를
쓰는 경우 `ImportResolverInterface`)에만 의존해서, 세 모드 모두 같은 4-Unit 구현체를
공유하고 목(mock)으로 독립적으로 테스트 가능하게 한다.

### 8.2 FileRunMode

- 파일 전체를 한 번에 읽어(존재하지 않으면 사용자 친화적 오류 메시지 출력 후 종료 코드
  반환) `RunPromptShell`처럼 tokenize→assemble→check→execute를 한 번 수행한다.
- `AssemblyError`/`CheckerError`/`ExecutorError`를 잡아 `SyntaxNode::getLine()`(또는
  `Token::line`)로 얻은 줄 번호와 함께 출력하고 **즉시 종료**한다(REPL처럼 다음 줄을
  계속 받지 않음).

### 8.3 DebugMode / Debugger — 실행 훅

Stmt 단위 stepping을 지원하려면 Executor가 "각 Statement를 실행하기 직전"에 외부로
제어권을 넘길 수 있어야 한다. `ExecuteInterface`의 공개 계약은 바꾸지 않고, `Executor`
구현체에 선택적 훅을 추가한다.

```cpp
// Executor/Executor.h
using StatementHook = std::function<void(Statement* stmt, const Environment& env)>;
void setStatementHook(StatementHook hook); // 미설정 시 REPL/File 모드는 기존과 동일 동작
```

- `Executor::execute(Statement*)` 내부, 실제 분기 처리 전에 `if (hook_) hook_(stmt, environment_);`
  를 호출한다. 훅은 필요하면 **블로킹**해서(표준 입력에서 디버그 명령을 읽는 루프) 다음
  진행 여부를 결정한다 — Executor 스레드를 그대로 사용하므로 별도 스레드/코루틴 없이
  구현 가능.
- `Environment`에 현재 스코프 체인을 읽기 전용으로 노출하는 introspection API를 추가한다:

```cpp
// Executor/Environment.h
struct ScopedVariable { std::string name; Value value; bool isGlobal; };
std::vector<ScopedVariable> currentScopeVariables() const; // inspect 용
```

- **`Debugger`**는 `StatementHook`으로 등록되는 콜백 객체다. 내부 상태:
  - `breakpoints_: std::set<int>` (줄 번호)
  - `watches_: std::vector<std::string>` (변수 이름)
  - `mode_: {Paused, Running}` 과 "다음에 멈출 조건"(step=바로 다음 문장에서 정지,
    next=현재 문장과 같은 깊이로 돌아올 때까지 진행, continue=다음 breakpoint까지 진행)
  - 훅이 호출될 때마다: 현재 줄이 breakpoint 집합에 있거나 `mode_`가 요구하는 정지 조건을
    만족하면, `watches_`에 있는 이름을 `env.lookup()`으로 조회해 출력하고, 표준 입력에서
    한 줄 명령을 읽어 `step`/`next`/`break N`/`breakpoints`/`remove N`/`continue`/
    `watch X`/`unwatch X`/`watches`/`inspect`를 파싱해 상태를 갱신한다(각 명령은 작은
    `Command` 객체로 만들어 처리하면 Command 패턴이 자연스럽게 적용됨 — §9).
  - "next가 블록 내부로 진입하지 않는다"는 요구는, 훅에 현재 실행 깊이(호출 스택 깊이 +
    블록 중첩 깊이)를 함께 넘기거나, Debugger가 진입한 시점의 깊이를 기억해 그보다 깊은
    곳에서 들어오는 훅 호출은 무시하는 방식으로 구현한다.

### 8.4 REPL 모드와의 관계

`RunPromptShell`은 변경하지 않는다(훅을 쓰지 않으므로 기존 `Executor` 사용 그대로 동작).
`DebugMode`만 `Executor`를 생성한 뒤 `setStatementHook`으로 `Debugger`를 등록하고 실행한다.

## 9. 새/변경 파일 요약

| Unit | 신규 파일 | 주요 변경 파일 |
|---|---|---|
| Tokenizer | — | `Token.h` (TokenType 추가) |
| Assembler | — | `SyntaxTree.h` (신규 노드, 자식 포인터 const 완화), `Assembler.h/.cpp` (신규 문법 파싱) |
| Checker | `Resolver.h/.cpp`(선택), `ConstantFolder.h/.cpp`(선택) | `Checker.h/.cpp` (함수/클래스/import 의미 검사, Resolver·ConstantFolder 호출 추가), `CheckerInterface.h`(ImportResolverInterface 의존 추가 시 생성자 시그니처 변경) |
| Executor | `FunctionObject.h`, `ClassObject.h`, `InstanceObject.h`, `ArrayObject.h`, `ModuleObject.h` | `Value.h/.cpp` (참조 타입 추가), `Environment.h/.cpp` (`*At` 계열, introspection), `Executor.h/.cpp` (신규 노드 핸들러, ReturnSignal, StatementHook, 내장 `Array`) |
| Import(신규) | `ImportResolverInterface.h`, `SourceLoaderInterface.h`, `ImportResolver.h/.cpp` | — |
| Shell | `FileRunMode.h/.cpp`, `DebugMode.h/.cpp`, `Debugger.h/.cpp`, `CommandLineArgs.h/.cpp` | `main.cpp` (모드 분기), `RunPromptShell.h/.cpp` (변경 없음 또는 Import 예외 처리 추가) |

프로젝트 파일(`CodeFab.vcxproj`/`.vcxproj.filters`)에는 위 신규 파일들을 새 `Import/` 필터와
기존 필터에 맞춰 추가해야 한다.

## 10. 적용 가능한 디자인 패턴

- **Interpreter / Visitor(변형)** — 이미 `Executor`가 `type_index → handler` 테이블로
  구현한 디스패치가 Visitor 패턴의 변형이다. 새 노드 타입(§2.2~2.6에서 추가된 것들)도
  `registerDefaultHandlers()`에 핸들러를 추가하는 것만으로 확장되므로 개방-폐쇄 원칙을
  그대로 유지한다.
- **Composite** — `SyntaxNode` 트리 구조 자체가 Composite 패턴이며, `ConstantFolder`가
  이를 그대로 활용한다.
- **Command** — Debugger의 각 디버그 명령(`step`/`break`/`watch` 등)을 `Command` 객체로
  캡슐화하면 명령 추가/조합이 쉬워진다.
- **Strategy** — `ImportResolver`가 주입받는 `SourceLoaderInterface`(실제 파일 시스템 vs.
  테스트용 인메모리 Fake)와, 정적 바인딩 성공/실패에 따라 `Environment` 조회 전략이
  `*At`(정적)과 `lookup`(동적) 사이에서 갈리는 부분이 Strategy 패턴에 해당한다.
- **Decorator(참고)** — 상수 폴딩을 "Assembler가 만든 트리를 감싸서 최적화된 트리를 내는
  단계"로 보면 파이프라인 단계 자체가 Decorator적 성격을 갖는다(다만 구현은 §6.2처럼
  제자리 치환으로 단순화).

## 11. 구현 순서 제안

1. **Token/SyntaxTree 확장** (§2.1, §2.2) — 다른 모든 작업의 전제조건. 기존 테스트가
   깨지지 않는지 먼저 확인.
2. **Value 확장 + 런타임 객체** (§2.4) — Function/Class/Instance부터. Array/Module은
   이후 단계에서 점진적으로 추가 가능.
3. **Function** (§3) — 가장 단순한 새 실행 단위. `ReturnSignal` 메커니즘을 여기서 먼저
   검증.
4. **Class** (§4) — Function 인프라(호출, 스코프, closure) 위에 필드/메서드/상속을 얹음.
5. **정적 배열** (§5) — 독립적이라 언제 넣어도 되지만 Function 인프라(내장 콜러블) 완성
   후가 자연스럽다.
6. **실행 전 최적화** (§6) — 1~5가 만든 새 노드 타입들이 Resolver/ConstantFolder 순회에서
   빠짐없이 처리되는지 함께 검증해야 하므로 마지막에 붙이는 편이 손이 덜 감.
7. **Import** (§7) — 4-Unit이 안정된 뒤에 붙여야 재귀 컴파일이 의미가 있음.
8. **공장 제어 쉘** (§8) — 전체 기능이 갖춰진 뒤 REPL 외 모드를 얹는 것이 자연스러움
   (디버그 모드가 결국 모든 새 노드 타입을 다 실행해볼 수 있어야 하므로).

## 12. 리스크와 하위 호환성

- **불변성 완화(§2.2)** 는 기존 `SyntaxNode`들의 `operator==`(값 비교)에는 영향이
  없다(비교 로직은 그대로 포인터가 가리키는 값을 따라가므로). 다만 "생성 후 트리 구조가
  바뀔 수 있다"는 전제가 새로 생기므로, 트리 구조에 의존하는 기존 테스트(특히
  `AssemblerTest`의 트리 동등성 비교)는 상수 폴딩이 켜진 이후에는 "폴딩되지 않은 원본
  트리"와 비교하도록 최적화 이전 단계의 결과만 검사하게 하거나, 폴딩 이후 트리를 기대값
  으로 갱신해야 한다.
- **`Executor` 내부 API 확장**(`SyntaxTree` 참조 보관, `StatementHook`)은 `ExecuteInterface`
  공개 계약을 바꾸지 않으므로 `RunPromptShell`, 기존 통합 테스트는 변경 없이 계속
  동작한다.
- **Checker 생성자 시그니처 변경**(`ImportResolverInterface&` 주입)은 `CheckerInterface`를
  통해 다형적으로 사용하는 `RunPromptShell` 쪽 코드에는 영향이 없지만, `Checker`를 직접
  생성하는 `main.cpp`와 `CheckerTest.cpp`의 생성자 호출부는 함께 수정해야 한다.
- **성능/메모리**: 상수 폴딩으로 죽은 노드가 `SyntaxTree`에 계속 쌓이는 트레이드오프는
  스크립트 하나의 실행 수명 동안만 유효하므로(REPL도 한 줄 = 한 `SyntaxTree`) 실질적인
  문제가 되지 않는다.
