# CodeFab Interpreter — 3일차 기능 확장 아키텍처

> 이 문서는 1일차 구현(현재 코드) 기준에서 3일차 요구사항(`3일차_CodeFab Interpreter.pdf`)의
> function / class / 정적 배열 / 실행전 최적화 / import / 공장 제어 쉘 을 지원하기 위해
> 필요한 아키텍처 변경을 설계한다. **이 문서는 설계 문서이며, 실제 `.h`/`.cpp` 코드는 아직
> 수정하지 않았다.** 구현 시 이 문서를 기준으로 각 Unit을 확장한다.
>
> 이번 개정판은 팀에서 합의한 구현 방향(아래)을 반영해, 이전 초안보다 구조를 단순화했다.
> - Function/Class는 **AST의 선언 노드(`FunctionDeclareStatement`/`ClassDeclareStatement`)
>   포인터 자체를 런타임 값으로 취급**한다. 별도의 `FunctionObject`/`ClassObject` 래퍼를
>   만들지 않는다.
> - 클래스 인스턴스의 필드, import 모듈의 export 목록은 **기존 `Scope` 클래스를 그대로
>   재사용**한다(새 map 타입을 만들지 않는다).
> - **정적 바인딩 결과(depth)는 `IdentifierExpression` 노드 자신에 직접 저장**한다
>   (`SyntaxTree` 쪽 별도 사이드 테이블을 두지 않는다).
> - **상수 연산 최적화는 Checker가 직접 계산하지 않고 `Executor::evaluate()`를 호출**해서
>   구한다. 이를 위해 `ExecuteInterface`에 `evaluate(Expression*)`를 추가한다.
> - **Import는 별도 Unit을 만들지 않고 Assembler가 파일을 읽어 재귀적으로 컴파일**한다.
>   여러 곳에서 같은 파일을 import할 때의 캐싱/중복 컴파일 최적화는 이번 범위에서 제외한다.
> - Class 상속(`Super`, `:`)은 이번 3일차 1차 구현 범위에서 제외한다. 아래 설계는 상속 없이도
>   자연스럽게 나중에 확장 가능한 형태로 잡아둔다.

## 0. 목표와 범위

- 기존 4-Unit 파이프라인(Tokenizer → Assembler → Checker → Executor)과 Shell의 책임 분리
  원칙을 그대로 유지한 채, 새 기능을 어느 Unit에 어떤 형태로 얹을지 정한다.
- 가능한 한 **기존 클래스의 시그니처를 유지**하고, 확장이 꼭 필요한 지점만 최소한으로
  변경한다.
- 팀이 이미 합의한 구현 방향(위 개정 요약)을 그대로 따르되, 왜 그 방향이 기존 구조와
  맞물리는지 근거를 함께 남긴다.

## 1. 파이프라인 개요

```
소스코드(string)
      │
      ▼
┌────────────┐  Token   ┌──────────────────────────┐  SyntaxTree  ┌───────────────────────┐  최적화된   ┌────────────┐
│ Tokenizer  │─────────▶│ Assembler                │─────────────▶│ Checker               │  SyntaxTree │ Executor   │
└────────────┘          │  - 문법 파싱               │               │  1) 의미 오류 검사        │────────────▶└────────────┘
                         │  - import 시 파일을 읽어    │               │  2) Resolver(정적바인딩)  │                  │
                         │    재귀적으로 자기 자신을    │               │  3) ConstantFolder      │                  │
                         │    호출해 컴파일           │               │     (Executor.evaluate  │                  │
                         └──────────────────────────┘               │      호출)              │                  │
                                                                     └───────────────────────┘                  │
                                                                                                                  │
                    공장 제어 쉘(Shell) : REPL mode / File mode / Debug mode 가 위 파이프라인을 감싼다  ◀───────────┘
```

새 Unit은 추가하지 않는다. import는 Assembler 내부 기능으로, 최적화는 Checker 내부
후처리 단계로 흡수한다.

## 2. 공통 기반 확장

### 2.1 Token / TokenType 추가

`Tokenizer/Token.h`의 `TokenType`에 아래를 추가한다.

| 분류 | 추가 Token |
|---|---|
| 키워드 | `FUNC`, `RETURN`, `CLASS`, `THIS`, `ARRAY`, `IMPORT`, `ALIAS`, `INSTANCEOF` |
| 구분자 | `DOT` (`.`), `LEFT_BRACKET` (`[`), `RIGHT_BRACKET` (`]`), `COMMA` (`,`) |

`COMMA`는 함수 파라미터/인자 목록에 필요하다. `Super`, `:`(상속)는 이번 범위에서 제외하므로
추가하지 않는다(§4.5에서 확장 지점만 남겨둔다).

### 2.2 SyntaxTree 노드 확장

`Assembler/SyntaxTree.h`에 아래 노드를 추가한다. 기존 노드와 동일하게 `SyntaxNode(tokens)`
생성자 패턴, `operator==` 오버라이드 관례를 따른다.

**Expression 계열**

- `CallExpression(callee, arguments)` — 함수 호출과 클래스 인스턴스 생성(`Robot()`)에
  동일하게 사용한다(둘 다 "식별자를 괄호로 호출"하는 동일한 문법이므로 노드도 하나로 통일).
- `FieldAccessExpression(object, name)` — `r.name`, `r.speed` 필드 읽기, 그리고
  `r.move(5)`처럼 메서드 호출의 callee 자리에도 그대로 쓰인다. **별도의 `GetExpression`/
  `SetExpression`을 나누지 않는다** — 대입 좌변으로 쓰일 때는 §2.4에서 설명하는 대로
  `AssignExpression`이 대상으로 이 노드를 그대로 받는다.
- `ThisExpression(keyword)`
- `ArrayExpression(sizeExpr)` — `Array(3)` 전용 문법. `Array`가 예약어(Token)이므로
  일반 `CallExpression`으로 파싱하지 않고, Assembler가 `ARRAY` 토큰을 보면 바로 이 노드를
  만든다(리터럴 파싱과 같은 층위).
- `IndexExpression(collection, index)` — `arr[i]` 읽기. 대입 좌변(`arr[i] = v`)일 때도
  `FieldAccessExpression`과 동일하게 `AssignExpression`이 대상으로 그대로 받는다.
- `InstanceOfExpression(object, className)`

**Statement 계열**

- `FunctionDeclareStatement(name, params, body)` — `Func add(a, b) { ... }`. 최상위
  함수 선언 전용 노드다.
- `MethodDeclareStatement(name, params, body)` — `move(dist) { ... }`처럼 클래스
  바디 안에서 `Func` 키워드 없이 `이름(params) { ... }` 형태로 선언되는 메서드 전용
  노드다(3일차 슬라이드의 실제 문법을 그대로 따름). `FunctionDeclareStatement`와 필드
  모양은 동일하지만(`name`/`params`/`body`), **문법이 서로 다르므로**(하나는 `FUNC`
  토큰으로 시작하고, 하나는 바로 식별자로 시작) 별도 타입으로 둔다 — 이렇게 하면
  Assembler가 "지금 클래스 바디 안이라 Func 없이 파싱"이라는 문맥을 노드 타입 자체로
  표현할 수 있고, Checker/Executor도 `dynamic_cast`로 "이건 메서드다"를 바로 구분할 수
  있다. 생성자도 별도 노드가 아니라 이름이 관례적으로 `init`인 평범한
  `MethodDeclareStatement`다.
- `ReturnStatement(keyword, value?)` — `value`는 없을 수 있다(nullptr 허용). 최상위
  함수와 메서드 모두 이 노드를 공유한다(둘 다 body가 `Statement*` 목록이라는 점은
  같으므로 return 문 자체를 분기할 필요는 없다).
- `ClassDeclareStatement(name, methods)` — `methods`는 `MethodDeclareStatement*`
  목록. (상속 확장 시 `superclass` 필드를 추가하면 됨 — §4.5)
- `ImportStatement(alias, declarations)` — `alias`는 식별자 Token, `declarations`는
  import 대상 파일에서 뽑아낸 최상위 `Statement*` 목록(주로 `VarDeclareStatement`,
  `FunctionDeclareStatement`). **파일을 읽고 파싱하는 일은 Assembler가 이 노드를 만드는
  시점에 이미 끝나 있다** — Checker/Executor는 다시 파일 시스템에 접근할 필요가 없다(§7).

**AssignExpression 대상 일반화**

기존 `AssignExpression`은 `IdentifierExpression* const identifier`만 대입 대상으로
받는다. 필드 대입(`r.speed = 10`)과 배열 원소 대입(`arr[i] = 7`)을 같은 노드로 표현하기
위해 대상 타입을 넓힌다.

```cpp
struct AssignExpression : public Expression {
    Expression* const target; // IdentifierExpression | FieldAccessExpression | IndexExpression
    Expression* const value;
    ...
};
```

Assembler가 대입 파싱 시 좌변으로 나온 표현식의 실제 타입이 위 세 가지 중 하나가 아니면
`AssemblerError`("잘못된 대입 대상")를 던진다(기존에도 "대입 좌변이 identifier가 아니면
오류"라는 검사가 있었으므로, 허용 목록만 셋으로 늘어나는 것뿐이다).

**노드에 정적 바인딩 결과(depth)를 직접 저장**

`IdentifierExpression`에 Checker가 채워 넣는 캐시 필드를 추가한다.

```cpp
struct IdentifierExpression : public Expression {
    const std::string name;
    mutable std::optional<int> depth; // Checker의 Resolver가 채운다. 못 찾으면 nullopt(전역/미해결)
    ...
};
```

`mutable`로 두는 이유: 노드의 "구문적 동일성"(`operator==`)은 여전히 `name`(과 토큰)만으로
결정되어야 하고, `depth`는 뒤에 실행되는 Checker 단계의 부가 정보일 뿐 노드의 정체성에
포함되지 않기 때문이다. `AssignExpression::target`이 `IdentifierExpression`인 경우도
**같은 노드 타입을 재사용**하므로 별도 필드 없이 그 노드의 `depth`를 그대로 채우면 읽기/쓰기
양쪽에 자동으로 적용된다.

**Statement가 걸쳐 있는 줄 번호 조회**

디버그 모드의 breakpoint 매칭(§8.3)을 위해 `SyntaxNode`가 자신이 포함한 모든 토큰의 줄
번호를 확인할 수 있어야 한다. 현재 `getLine()`은 `tokens.front().line`(첫 토큰 줄)만
반환하므로, 아래를 추가한다.

```cpp
// Assembler/SyntaxTree.h
class SyntaxNode {
public:
    ...
    int getLine() const { return tokens.empty() ? -1 : tokens.front().line; }
    bool containsLine(int line) const {
        for (const auto& t : tokens) if (t.line == line) return true;
        return false;
    }
};
```

`tokens`는 이미 `SyntaxNode`가 생성자에서 받아 보관하고 있으므로(현재도 `private const
std::vector<Token> tokens;`), 이 메서드는 새 상태 없이 그 위에 얹는 조회 함수다.

### 2.3 Value 확장 — Function/Class/Instance/Array/Module

`Executor/Value.h`의 `Type` enum과 내부 `std::variant`에 아래를 추가한다. 새 러타임
객체 클래스를 최소화하기 위해, 가능한 곳은 **AST 노드 포인터**와 **기존 `Scope` 클래스**를
그대로 재사용한다.

```cpp
enum class Type { Nil, Boolean, Number, String, Function, Class, Instance, Array, Module };

std::variant<std::monostate, bool, double, std::string,
             const FunctionDeclareStatement*,      // Function: 선언 노드를 그대로 참조
             const ClassDeclareStatement*,          // Class: 선언 노드를 그대로 참조
             std::shared_ptr<InstanceValue>,        // Instance: 아래 참고
             std::shared_ptr<ArrayValue>,           // Array: 아래 참고
             std::shared_ptr<Scope>>                // Module: import한 파일의 export 스코프
    data_;
```

- **함수 값 / 클래스 값**은 AST가 프로그램 실행 내내 살아있는 `SyntaxTree`가 소유하고
  있으므로(참조만 하고 소유하지 않아도 안전), 원시 포인터로 충분하다. 별도의
  `FunctionObject`/`ClassObject` 래퍼가 필요 없다.
- **`InstanceValue`**(경량 구조체, `Executor/InstanceValue.h`) :
  ```cpp
  struct InstanceValue {
      const ClassDeclareStatement* klass;
      std::shared_ptr<Scope> fields; // 필드 저장소. Scope::define/assign/get 그대로 사용
  };
  ```
  "필드 읽기/쓰기/없는 필드는 새로 생성"이라는 요구사항이 `Scope`의 기존 동작
  (`define`은 항상 upsert, `get`은 없으면 `nullopt`)과 정확히 일치하므로 새 자료구조를
  만들 필요가 없다. `Scope`를 `shared_ptr`로 감싸는 이유는 인스턴스가 여러 곳에서 참조돼도
  같은 필드 저장소를 공유해야 하기 때문이다(참조 의미론).
- **`ArrayValue`**(경량 구조체, `Executor/ArrayValue.h`) :
  ```cpp
  struct ArrayValue { std::vector<Value> items; };
  ```
  고정 크기이므로 생성 후 크기를 바꾸는 API는 두지 않는다.
- **Module**은 별도 타입을 만들지 않고 **`std::shared_ptr<Scope>`를 그대로 `Value`에
  저장**한다. import한 파일의 최상위 선언들을 이 `Scope`에 실행해 넣으면, 이후
  `alias.add(...)` 같은 접근이 인스턴스 필드 접근과 완전히 같은 코드 경로
  (`FieldAccessExpression` 평가 시 "무언가의 Scope에서 이름을 찾는다")를 타게 만들 수
  있다(§7.3).

`Environment`/`Scope` 자체는 변경할 필요가 없다(값 타입으로 `Value`를 그대로 다룸). 단,
§6.1에서 정적 바인딩을 위한 거리 기반 접근 메서드를 추가한다.

## 3. Function 지원 설계

### 3.1 Assembler

- `parseDeclaration()`에 `FUNC` 분기 추가 → `parseFunction()`이
  `FunctionDeclareStatement(name, params, body)`를 만든다. `body`는 기존 블록 파싱을
  재사용한다. 파라미터 목록은 `COMMA`로 구분해서 읽는다.
- Primary 표현식을 파싱한 뒤, 이어지는 `(`/`.`/`[`를 반복적으로 처리하는 "postfix 체인"
  파싱을 Call 레벨에 추가한다(§4.1, §5.1과 공유하는 동일한 루프):
  ```
  expr = primary()
  loop:
    if 다음이 "(" : expr = CallExpression(expr, args)
    elif 다음이 "." : expr = FieldAccessExpression(expr, name)
    elif 다음이 "[" : expr = IndexExpression(expr, index)
    else break
  ```
  이렇게 하면 `add(1,2)`, `r.move(5)`, `arr[i]`, 그리고 이들의 조합(`r.list[0]()`처럼)이
  모두 하나의 루프에서 좌결합으로 파싱된다.
- `return` 문 파싱: `RETURN` 토큰 소비 후 바로 `;`가 오면 `value=nullptr`, 아니면 표현식을
  파싱한다.

### 3.2 Checker

- 스코프 스택에 "현재 함수(또는 메서드) 안인지" 카운터를 추가해, `ReturnStatement`가
  함수 바깥에서 나타나면 `CheckerError`.
- `FunctionDeclareStatement` 검사 시 파라미터 이름 집합에 중복이 있으면 `CheckerError`.
- **호출 대상이 함수가 맞는지, 인자 개수가 맞는지는 원칙적으로 런타임에만 확정된다**
  (`var x = "hi"; x();`처럼 변수에 무엇이 들었는지는 실행해봐야 앎). 이 두 검사는
  Executor의 런타임 오류로 둔다(§3.3). 다만 같은 스코프에서 바로 보이는
  `FunctionDeclareStatement`를 직접 호출하는 뻔한 경우(`add(1)` 같은 인자 개수 불일치)는
  Checker가 부가적으로 미리 잡아줄 수도 있으나 필수는 아니다(있으면 좋은 정도).

### 3.3 Executor

- `FunctionDeclareStatement` 실행(다른 Statement들처럼 프로그램 흐름 중 한 번 지나감):
  `environment.define(name, Value(&stmt))`로 함수 이름을 현재 스코프에 등록한다. 이
  시점에 **함수 이름이 먼저 등록되므로 재귀 호출이 자연스럽게 동작**한다(자기 자신을 부르는
  코드는 함수가 실제로 호출될 때만 실행되고, 그때는 이미 이름이 등록되어 있다).
- `CallExpression` 핸들러 (callee가 `IdentifierExpression`인 경우):
  1. callee를 평가해 `Value`를 얻는다.
  2. `Function` 타입이면: 새 스코프를 `environment.pushScope()`로 하나 push하고, 인자
     개수와 파라미터 개수가 다르면 `ExecutorError`, 같으면 각 파라미터 이름에 평가된
     인자 값을 `define`, `body`의 각 `Statement`를 실행한다.
  3. `Class` 타입이면 §4.3의 인스턴스 생성 로직으로 위임한다.
  4. 그 외 타입이면 `ExecutorError`("호출할 수 없는 대상입니다").
- "새 스코프 push → 파라미터 bind(+ 있으면 `this` bind) → body 실행 → `ReturnSignal`
  캐치 → 스코프 pop"이라는 호출 절차 자체는 최상위 함수(`FunctionDeclareStatement`)와
  클래스 메서드(`MethodDeclareStatement`, §4.3)가 완전히 동일하다. 두 노드 타입이
  달라서 코드가 중복되지 않도록, `name`/`params`/`body`만 받는 공용 내부 헬퍼(예:
  `Value invoke(const Token& name, const std::vector<Token>& params, const
  std::vector<Statement*>& body, const std::vector<Value>& args,
  std::optional<Value> boundThis)`)로 뽑아서 두 CallExpression 경로(일반 호출,
  §4.3의 메서드 호출)가 함께 호출하는 것을 권장한다.
- **`this`가 없는 일반 함수는 호출 시 현재 스코프 스택(호출부의 지역 스코프 포함) 바로
  위에 새 스코프를 얹는 가장 단순한 방식을 쓴다.** 즉, 클로저(선언 시점 환경 캡처)는
  구현하지 않는다 — 함수는 자신의 파라미터와 전역만 참조하는 것을 전제로 하고, 호출부의
  지역 변수를 우연히 참조할 수 있는 것은 알려진 단순화(동적 스코프에 가까운 동작)로
  문서화해둔다. 3일차 요구사항 범위에서는 클로저가 필요하지 않으므로 이 단순화로 충분하다.
- **return 처리**: `ReturnStatement` 실행 시 내부 전용 제어 흐름 신호
  `Executor::ReturnSignal{ Value value }`(공개 API가 아님, `std::exception`을 상속하지
  않는 순수 C++ 예외로 구현해 일반 `ExecutorError`와 섞이지 않게 한다)를 던지고, 함수 호출
  처리부가 `catch (ReturnSignal&)`로 받아 그 값을 `CallExpression`의 평가 결과로 사용한다.
  `return`이 없거나 `return;`(빈 값)이면 `Nil`.
- 함수 호출이 끝나면(정상 종료든 `ReturnSignal`을 캐치했든) push했던 스코프를 반드시
  `popScope()`한다(성공/예외 양쪽 경로 모두 커버 — RAII 가드 클래스를 만들어 두면 안전).

## 4. Class 지원 설계

이번 범위는 **상속 없는 클래스**(필드, 메서드, `this`, 생성자 `init`)까지다.

### 4.1 Assembler

- `parseDeclaration()`에 `CLASS` 분기 추가. `Class Name { method* }`를 파싱해
  `ClassDeclareStatement`를 만든다. `method*`는 `FUNC` 키워드 없이 바로
  `IDENTIFIER LEFT_PAREN params? RIGHT_PAREN blockStmt` 형태로 파싱해서
  `MethodDeclareStatement`를 만든다 — 최상위 함수 파싱(`parseFunction`, `FUNC` 토큰
  소비 포함)과는 진입 조건이 다르지만, `FUNC` 토큰 소비 한 줄만 빠질 뿐 나머지
  (이름/파라미터 목록/바디 파싱) 로직은 그대로 재사용할 수 있다(공용 헬퍼로 뽑아
  `parseFunction`과 `parseMethod`가 함께 호출하게 만들면 중복이 없다).
- Primary 표현식에 `THIS` 분기 추가: `This` → `ThisExpression`.
- §3.1에서 추가한 postfix 체인의 `.` 분기가 `FieldAccessExpression`을 만든다. 이 체인
  바로 다음에 `=`이 오면(대입 파싱 레벨에서) 좌변이 `FieldAccessExpression`인
  `AssignExpression`을 만든다(§2.2의 대상 일반화).

### 4.2 Checker

- "현재 클래스/메서드 안인지" 스택을 추가해, 클래스 바깥에서 `This` 사용 시
  `CheckerError`.
- `init` 메서드에서 값 있는 `return <expr>;` 사용 시 `CheckerError`("init은 return 없음").
  빈 `return;`(조기 종료) 허용 여부는 팀 컨벤션으로 정하되, 기본값은 금지로 한다(요구사항
  문구 "init 은 return 허용 x"를 그대로 따름).
- "인스턴스가 아닌 대상의 필드 접근", "존재하지 않는 필드/메서드 접근"은 실행해봐야
  아는 값의 실제 타입에 달려 있으므로 Executor의 런타임 오류로 둔다.

### 4.3 Executor

- `ClassDeclareStatement` 실행: `environment.define(name, Value(&stmt))` (함수와
  완전히 동일한 패턴).
- `CallExpression`의 callee가 `Value::Type::Class`이면(§3.3의 3번):
  1. `auto fields = std::make_shared<Scope>();`
  2. `InstanceValue{ klass, fields }`를 만들어 `Value`로 감싼다.
  3. `klass->methods`에서 이름이 `"init"`인 메서드를 찾으면, §3.3의 함수 호출 로직을
     재사용하되 새로 push하는 스코프에 `environment.define("this", instanceValue)`를
     먼저 실행한 뒤 파라미터를 bind한다. `init`의 반환값은 버리고 **항상 새 인스턴스를
     반환**한다.
- `FieldAccessExpression` 평가(호출이 아닌 값 읽기 문맥):
  1. `object`를 평가해 `Instance`가 아니면 `ExecutorError`.
  2. `instance.fields->get(name)`이 있으면 그 값을 반환.
  3. 없으면 `instance.klass->methods`에서 이름이 일치하는
     `MethodDeclareStatement`를 찾는다 — 있으면 **"바인딩된 메서드"** 를 표현할 별도
     타입을 새로 만들지 않고, 이 경우는 항상 바로 다음에 `CallExpression`의 callee로만
     쓰인다는 점을 이용해 **`CallExpression` 핸들러가 callee 표현식 자체가
     `FieldAccessExpression`인지를 먼저 검사**하도록 한다(아래).
  4. 필드에도 메서드에도 없으면 `ExecutorError`("존재하지 않는 필드/메서드").
- `CallExpression` 핸들러에 메서드 호출 경로 추가: **callee가
  `FieldAccessExpression(object, name)`이면**, 먼저 `object`를 평가해 `Instance`가
  아니면 `ExecutorError`, 맞으면 `instance.klass->methods`에서 `name`을 찾아(없으면
  `ExecutorError`) §3.3의 함수 호출 로직을 재사용하되 `environment.define("this",
  instanceValue)`를 먼저 실행한 뒤 파라미터를 bind한다. **필드 값이 우연히 함수 타입을
  담고 있어도(값으로 저장된 함수) 이 경로에서는 메서드로 취급하지 않는다** — 필드에 저장된
  콜러블 값을 호출하고 싶으면 먼저 `FieldAccessExpression`으로 평가해 `Function` 값을
  얻은 뒤 일반 `CallExpression(callee=그 값)`으로 호출하게 되는데, 현재 문법상
  callee는 항상 표현식이므로 이 케이스는 "callee가 FieldAccessExpression"으로 잡혀
  위 메서드 경로를 타게 된다. 필드 vs 메서드 우선순위(필드가 있으면 필드를 함수처럼
  호출)가 필요해지면 이 지점에서 `instance.fields->get(name)`을 먼저 확인하도록
  한 줄만 추가하면 되므로 확장 지점으로 남겨둔다.
- `AssignExpression`의 target이 `FieldAccessExpression`이면: `object`를 평가해
  `Instance`가 아니면 `ExecutorError`, 맞으면 `instance.fields->define(name, value)`
  (없던 필드면 새로 생성 — `Scope::define`의 기존 동작 그대로).
- `ThisExpression` 평가: 함수와 동일하게, 메서드 호출 시 새로 push한 스코프의
  `"this"`라는 이름으로 바인딩된 값을 **일반 변수 조회와 같은 경로**(§6.1의 정적 바인딩
  대상)로 읽는다. 즉 `This`도 파싱 시 내부적으로 `IdentifierExpression("this")`와
  동일하게 다루면(또는 `ThisExpression`을 별도 노드로 두되 평가 로직만
  `IdentifierExpression`과 동일하게 구현하면) depth 캐싱 이점을 그대로 받을 수 있다.

### 4.4 인스턴스 저장소를 별도로 두는 이유

"Class에 대한 Environment도 따로 가지고 있어야 할 듯"이라는 방향을 `InstanceValue.fields:
shared_ptr<Scope>`로 구체화했다. 인스턴스 필드는 프로그램의 렉시컬 스코프 체인
(`Environment`가 관리하는 블록 스코프 스택)과는 완전히 독립적인 저장소이기 때문에, 함수
호출용 스코프 스택에 끼워 넣지 않고 인스턴스 하나당 별도의 `Scope` 인스턴스를 붙이는 것이
정확하다(`Environment`는 "현재 실행 중인 코드 블록들의 스코프 스택"이고, `InstanceValue.fields`는
"이 객체가 살아있는 동안 유지되는 필드 테이블"로 수명 주기가 다르다).

### 4.5 상속 확장 지점(이번 범위 제외)

나중에 상속을 추가할 때는: `ClassDeclareStatement`에 `superclass:
IdentifierExpression*`(nullable) 필드 추가, `Class B : A` 파싱을 위한 `COLON` 토큰과
문법 추가, `SUPER` 토큰과 `SuperExpression` 노드 추가, `InstanceValue`는 변경 없이 그대로
두고 메서드 탐색만 `klass->methods` → 없으면 `klass->superclass->methods`로 재귀
확장하면 된다. 필드 저장소(`Scope`)는 상속 여부와 무관하게 인스턴스당 하나로 계속 충분하다.

## 5. 정적 배열 설계

### 5.1 Assembler

- `ARRAY`가 예약어이므로 Primary 표현식 파싱에 전용 분기를 추가한다: `ARRAY` 토큰을
  보면 `(`를 기대하고 크기 표현식을 파싱한 뒤 `)`를 기대해서 `ArrayExpression(sizeExpr)`를
  만든다(일반 `CallExpression`으로 파싱하지 않음 — 문법이 아예 다른 키워드 기반 리터럴).
- §3.1에서 만든 postfix 체인의 `[` 분기가 `IndexExpression(collection, index)`를
  만든다. 대입 좌변이면 §2.2대로 `AssignExpression`의 target이 된다.

### 5.2 Checker

- `Array(n)`에서 `n`이 리터럴 상수인 경우 타입이 숫자가 아니면 미리 잡아줄 수도 있지만,
  변수를 통해 크기를 넘기는 경우(`Array(x)`)는 실행해봐야 알 수 있으므로 필수 검사는
  아니다. 배열 관련 오류는 대부분 Executor의 런타임 오류로 둔다(요구사항 문서도 전부
  "런타임 오류"로 명시).

### 5.3 Executor

- `ArrayExpression` 평가: `sizeExpr`를 평가해 `Number`가 아니면 `ExecutorError`("배열의
  사이즈는 반드시 number"). 정수로 변환해(음수/소수 처리 정책은 팀 결정, 기본은 음수/소수는
  오류) `ArrayValue{ items(size, Value()) }`(전부 `Nil`)를 만들어 `Array` 타입 `Value`로
  반환한다.
- `IndexExpression` 평가(읽기): `collection`을 평가해 `Array`가 아니면
  `ExecutorError`("index 접근은 오직 배열만 지원"). `index`를 평가해 `Number`가 아니면
  `ExecutorError`. 정수 변환 후 `[0, size)` 범위를 벗어나면 `ExecutorError`("인덱스 범위
  벗어남"). 그 외에는 `items[index]`.
- `AssignExpression`의 target이 `IndexExpression`이면 위와 같은 검사 후
  `items[index] = value`.

## 6. 실행 전 최적화 설계

두 최적화 모두 `Checker::check()`가 "의미 오류 검사"를 통과한 뒤에만 수행하는
**후처리 하위 단계**로 구현한다.

```
Checker::check(SyntaxTree& tree)
   1) SemanticAnalyzer  — 기존 의미 오류 검사 + §3~4의 새 검사들
   2) Resolver          — 통과 시: IdentifierExpression::depth 채우기
   3) ConstantFolder    — 통과 시: Executor::evaluate()를 호출해 순수 상수 서브트리를 리터럴로 치환
```

세 하위 단계는 처음엔 `Checker.cpp` 안의 private 헬퍼 함수/클래스로 두고, 커지면
`Checker/Resolver.h/.cpp`, `Checker/ConstantFolder.h/.cpp`로 분리한다.
`CheckerInterface`의 공개 시그니처(`bool check(SyntaxTree&)`)는 바뀌지 않는다.

### 6.1 Resolver (정적 바인딩)

- 기존 `SemanticAnalyzer`가 이미 "중복 선언" 검사를 위해 스코프 스택(블록마다 선언된
  이름 집합)을 관리하므로, 같은 순회에 편승해서 "몇 단계 위 스코프인지" 깊이를 함께
  추적한다.
- `IdentifierExpression`을 만날 때마다(변수 참조로 쓰이든, `AssignExpression`/
  `ThisExpression`의 대상으로 쓰이든 — §2.2, §4.3에서 이들이 모두 같은 노드 타입을
  재사용하도록 설계했으므로 Resolver는 `IdentifierExpression`만 처리하면 된다): 현재
  스코프부터 바깥쪽으로 몇 단계를 올라가야 그 이름이 선언된 스코프가 나오는지 세어
  `node->depth = distance;`에 **직접 기록**한다. 로컬 스코프 어디에서도 못 찾으면
  (전역이거나, import 모듈 이름) `depth`를 `nullopt`로 남겨 두고, Executor가 기존 방식대로
  전역까지 훑는 동적 조회로 폴백하게 한다.
- `Environment`에 거리 기반 접근 메서드를 추가한다.

```cpp
// Executor/Environment.h
std::optional<Value> lookupAt(int distance, const std::string& name) const;
bool assignAt(int distance, const std::string& name, const Value& value);
```

  `distance`만큼 `scopes_`를 안쪽에서부터 건너뛰어 정확히 그 `Scope`에서만
  조회/대입하므로 스코프 깊이에 무관한 상수 시간 접근이 된다. Executor의
  `evaluate(IdentifierExpression*)`/대입 처리부는 `expr->depth`가 있으면 `*At` 계열을,
  없으면 기존 `lookup`/`assign`(동적 조회)을 호출하도록 분기한다.

### 6.2 ConstantFolder (상수 연산 최적화) — Executor::evaluate() 재사용

- 상수 연산 최적화는 Checker가 산술 규칙을 다시 구현하지 않고, **실제 실행 로직인
  `Executor::evaluate()`를 그대로 호출**해서 값을 구한다. 이를 위해 `evaluate`를
  `ExecuteInterface`의 공개 계약으로 승격한다.

```cpp
// Executor/ExecuteInterface.h
class ExecuteInterface {
public:
    virtual ~ExecuteInterface() = default;
    virtual void execute(SyntaxTree& tree) = 0;
    virtual Value evaluate(Expression* expr) = 0; // 신규: 단일 표현식 평가
};
```

- `Checker`는 생성자에서 `ExecuteInterface&`를 주입받는다(새 의존성). `ConstantFolder`는
  Assembler가 만든 트리를 **bottom-up으로 재귀 순회**하며(§2.2에서 자식 포인터를 이미
  const가 아닌 일반 포인터로 완화했다고 가정 — 아래 "노드 불변성 완화" 참고), 자식을 먼저
  접은 뒤 부모가 `BinaryExpression`류이고 두 자식이 모두 리터럴
  (`NumberExpression`/`BooleanExpression`/`StringExpression`)이면:
  1. `try { Value v = executeInterface.evaluate(binaryExpr); }` 로 실제 값을 계산한다.
     리터럴만 있는 서브트리이므로 `evaluate` 내부에서 `Environment`를 전혀 건드리지
     않아 부작용 없이 안전하게 호출할 수 있다.
  2. 계산이 성공하면 `v`의 타입에 맞는 새 리터럴 노드(`NumberExpression` 등)를 만들어
     `tree.add(...)`로 소유권을 등록하고, **부모의 `left`/`right`(또는 해당 자식 필드)를
     새 노드로 덮어쓴다.**
  3. `evaluate`가 `ExecutorError`를 던지면(예: 0으로 나누기) **폴딩하지 않고 원래
     트리를 그대로 둔다** — 컴파일 시점에 오류를 대신 내면 안 되고, 여전히 Executor가
     런타임에 그 지점에서 오류를 내야 줄 번호/문맥이 올바르다.
- **노드 불변성 완화**: 위 치환을 위해 `BinaryExpression::left/right`,
  `UnaryExpression::operand` 등 "자식을 가리키는" 필드는 `Expression* const`에서
  `Expression*`(비-const)로 완화해야 한다. 다른 필드(`name` Token 등 구조를 바꾸지 않는
  필드)는 기존처럼 `const`를 유지한다. 원래 있던 서브트리 노드들은 더 이상 참조되지 않지만
  `SyntaxTree`가 계속 소유하고 있으므로 메모리 안전성은 깨지지 않는다.
- 폴딩 대상은 순수 산술/비교 연산으로 한정된다 — 함수 호출, 대입, `This`, 변수 참조 등은
  애초에 "두 자식이 모두 리터럴"이라는 조건을 만족하지 못하므로 자동으로 제외된다.

### 6.3 테스트 전략 (Test Double)

- **Resolver 검증**: `Environment`를 감싸 `lookupAt`/`assignAt` 호출 횟수와
  `lookup`/`assign`(동적 조회) 호출 횟수를 세는 스파이(spy)를 만든다. 중첩 블록 안에서
  지역 변수를 사용하는 스크립트를 실행했을 때 동적 조회가 전혀 발생하지 않는지 단언한다.
- **ConstantFolder 검증**: `ExecuteInterface`의 **가짜(Fake) 구현**을 Checker에 주입해
  `evaluate()` 호출 횟수를 세거나, 진짜 `Executor`를 감싸 카운트하는 스파이를 만든다.
  최적화 전/후로 실행했을 때 "연산 횟수가 N회 → 0회"로 줄었는지 확인한다. 트리 구조
  검증(폴딩 후 `BinaryExpression`이 사라지고 리터럴 노드 하나만 남았는지)은
  `AssemblerTest`처럼 트리를 직접 비교(`operator==`)해서 확인할 수 있다.

## 7. Import 설계 — Assembler가 파일을 읽어 재귀 컴파일

별도 Unit을 만들지 않고, **Assembler가 import 대상 파일을 읽고 재귀적으로 자기 자신을
호출해 컴파일**한다. import는 문법적으로 "다른 파일의 선언들을 현재 파일의 한 지점에
가져와 붙이는 것"이므로, "소스 → 문법 트리를 조립한다"는 Assembler의 책임 범위 안에
자연스럽게 들어간다.

### 7.1 Assembler 의존성 확장

`Assembler`는 이제 `import`를 처리하려면 (1) 파일 내용을 읽고 (2) 그 내용을 토큰화해야
한다. 이 두 단계를 **Assembler가 직접 하지 않고 `SourceReaderInterface` 하나에
위임**한다 — `SourceReaderInterface`가 내부적으로 `TokenizeInterface`를 들고 있다가
`read(path)` 호출 시 파일을 읽고 그 자리에서 토큰화까지 마친
`std::vector<Token>`을 반환한다. 그 결과 **Assembler는 `SourceReaderInterface`
하나만 주입받으면 된다** — Tokenizer에 대한 의존은 `SourceReaderInterface` 구현체
안으로 완전히 캡슐화된다.

```cpp
// Assembler/SourceReaderInterface.h  (신규, 테스트에서 Fake로 대체하기 위한 최소 추상화)
struct SourceReaderInterface {
    virtual ~SourceReaderInterface() = default;
    // path의 소스를 읽어 토큰화까지 마친 결과를 반환한다. 파일이 없으면 예외.
    virtual std::vector<Token> read(const std::string& path) = 0;
};
```

```cpp
// Assembler/FileSourceReader.h  (SourceReaderInterface의 실제 파일 시스템 구현체)
class FileSourceReader : public SourceReaderInterface {
public:
    explicit FileSourceReader(TokenizeInterface& tokenizer);
    std::vector<Token> read(const std::string& path) override;
private:
    TokenizeInterface& tokenizer_; // 파일 내용을 읽은 뒤 이걸로 토큰화한다
};
```

```cpp
// Assembler/Assembler.h
class Assembler : public AssemblerInterface {
public:
    explicit Assembler(SourceReaderInterface& sourceReader);
    ...
private:
    std::vector<std::string> importStack_; // 순환 import 검출용
};
```

`main.cpp`는 `Tokenizer`를 먼저 만들고, 그걸 `FileSourceReader`에 주입하고, 그
`FileSourceReader`를 다시 `Assembler`에 주입한다(생성 순서: `tokenizer` →
`sourceReader(tokenizer)` → `assembler(sourceReader)`). `AssemblerTest`/`CheckerTest`는
`SourceReaderInterface`의 인메모리 Fake(맵 기반: path → 미리 만들어둔
`vector<Token>`)를 주입해 파일 시스템과 실제 Tokenizer 없이도 import 관련 케이스를
테스트할 수 있다.

### 7.2 import 문 파싱과 재귀 컴파일

- `import STRING alias IDENTIFIER ;` 를 파싱하는 도중 Assembler는:
  1. 문자열 리터럴 `path`가 이미 `importStack_`에 있으면 `AssemblerError`("순환
     import").
  2. `importStack_.push_back(path)`.
  3. `sourceReader_.read(path)`로 파일을 읽고 토큰화까지 완료된
     `std::vector<Token>`을 받는다(파일이 없으면 `SourceReaderInterface` 구현체가
     던지는 예외를 `AssemblerError`("import 대상 파일 없음")로 감싸 다시 던진다).
  4. 받은 토큰으로 `assemble(tokens)`를 **재귀 호출**해서 그 파일의 `SyntaxTree`를
     얻는다(그 파일 안에 또 `import`가 있으면 자연스럽게 더 재귀된다 — `importStack_`을
     인스턴스 멤버로 두었기 때문에 순환은 재귀 스택 전체에서 감지된다).
  5. 결과 트리의 최상위 문장들 중 `VarDeclareStatement`/`FunctionDeclareStatement`만
     추려 `ImportStatement(alias, declarations)`에 담는다(그 외 문장이 섞여 있으면
     팀 컨벤션에 따라 무시하거나 `AssemblerError` — "선언 외 내용은 허용하지 않음"을
     엄격 모드로 두는 것을 권장).
  6. `importStack_.pop_back()`.
- **캐싱은 이번 범위에서 하지 않는다** — 같은 파일을 여러 곳에서 import하면 매번
  다시 읽고 다시 파싱한다(정확성에는 문제없고, 성능 최적화는 나중 과제로 명시적으로
  미룬다).

### 7.3 Checker / Executor

- Checker는 파일을 다시 열 필요가 없다(이미 Assembler가 존재 여부/순환을 확인했다).
  아래만 스코프 스택을 보며 정적으로 검사한다:
  - `ForStatement` 바디 내부에 `ImportStatement`가 있으면 `CheckerError`.
  - 같은 스코프에서 동일 alias(또는 동일 path) 중복 import → `CheckerError`.
  - 상위 스코프에서 이미 import한 path를 하위 스코프가 다시 import → `CheckerError`.
  - alias 이름이 같은 스코프의 다른 선언과 충돌하면 일반 변수 중복 선언과 동일하게 처리.
- Executor는 `ImportStatement` 실행 시:
  1. `auto moduleScope = std::make_shared<Scope>();`
  2. `declarations`의 각 `Statement`를 **그 스코프를 대상으로** 실행한다(함수 호출 시
     새 스코프를 push하는 것과 동일한 메커니즘 — `environment.pushScope()`로
     `moduleScope`를 얹고 실행 후 pop하되, `moduleScope` 자체는 `shared_ptr`로 계속
     살아있게 한다).
  3. `environment.define(alias, Value(moduleScope))` — `Module` 타입 값으로 alias를
     현재 스코프에 등록한다.
- `FieldAccessExpression`/`CallExpression`이 object를 평가한 결과가 `Module`이면,
  `InstanceValue.fields` 대신 그 `shared_ptr<Scope>`에서 바로 `get(name)`한다 — §4.3의
  필드/메서드 조회와 사실상 같은 코드 경로(둘 다 "어떤 `Scope`에서 이름을 찾는다")이므로,
  구현 시 이 둘을 하나의 작은 헬퍼로 묶어도 좋다(필수는 아님).

## 8. 타입 검사 연산자 (instanceof)

### 8.1 Assembler

- `INSTANCEOF`를 비교 연산자와 비슷한 우선순위의 이항 연산자로 파싱하되, 우변이
  식별자가 아니면 파싱 오류. `InstanceOfExpression(object, className)`을 만든다.

### 8.2 Executor

- 좌변을 평가해 `Instance`가 아니면 `false`를 반환한다(타입 오류로 처리할지는 팀 결정,
  다른 언어의 일반적 관례를 따라 `false` 권장). `Instance`이면 우변 이름으로 환경에서
  `Class` 값을 찾아 `instance.klass == 그 ClassDeclareStatement*`인지 포인터 비교한다.
- 상속이 없는 현재 범위에서는 이 비교가 전부다. §4.5에서 상속이 추가되면
  `instance.klass`부터 `superclass` 체인을 따라 올라가며 비교하도록 자연스럽게 확장된다.

## 9. 공장 제어 쉘 설계

### 9.1 모드 분리

```
Shell/
├── RunPromptShell.h/.cpp        기존 REPL 모드 (변경 없음, 그대로 재사용)
├── FileRunMode.h/.cpp           신규: 파일 모드
├── DebugMode.h/.cpp             신규: 디버그 모드
├── Debugger.h/.cpp              신규: 디버그 모드의 명령 처리기(step/break/watch 등)
├── CommandLineArgs.h/.cpp       신규: argv 파싱 (테스트하기 쉽게 main.cpp에서 분리)
└── main.cpp                     CommandLineArgs 로 모드를 고른 뒤 위임
```

`FileRunMode`/`DebugMode`도 `RunPromptShell`과 마찬가지로 4개 `*Interface`에만 의존해서
같은 4-Unit 구현체를 공유하고, mock으로 독립적으로 테스트 가능하게 한다.

### 9.2 FileRunMode

- 파일 전체를 한 번에 읽어(존재하지 않으면 오류 메시지 출력 후 종료) tokenize→assemble→
  check→execute를 한 번 수행한다. 예외 발생 시 `SyntaxNode::getLine()`으로 얻은 줄
  번호와 함께 출력하고 즉시 종료한다.

### 9.3 DebugMode / Debugger

Stmt 단위 stepping을 위해 `ExecuteInterface`에 디버그용 접점 두 개를 추가한다(§6.2에서
이미 `evaluate`를 추가했으므로, 여기서는 환경 조회와 스텝 훅을 추가).

```cpp
// Executor/ExecuteInterface.h
class ExecuteInterface {
public:
    ...
    virtual const Environment& environment() const = 0; // watch/inspect 용
};
```

```cpp
// Executor/Executor.h (구현체 전용, 인터페이스에는 없음 — Debug 모드만 이 구체 타입을 씀)
using StatementHook = std::function<void(Statement* stmt)>;
void setStatementHook(StatementHook hook); // 미설정 시 REPL/File 모드는 기존과 동일 동작
```

- `Executor::execute(Statement*)` 내부, 실제 분기 처리 **직전**에
  `if (hook_) hook_(stmt);`를 호출한다. 훅은 필요하면 표준 입력에서 디버그 명령을 읽는
  루프를 돌며 **블로킹**한다 — Executor 실행 스레드를 그대로 쓰므로 별도 스레드 없이
  구현 가능하다.
- **watch는 훅 매개변수로 Environment를 넘기지 않고, `Debugger`가 필요할 때
  `executor.environment()`를 호출해서 가져온다** — 매 statement마다 환경 스냅샷을
  복사해서 넘기지 않아도 되므로 더 가볍다. `Environment`에 현재 스코프 체인을 읽기
  전용으로 노출하는 조회 메서드(`lookup(name)`은 이미 있음)만 있으면 watch 목록의 각
  이름을 조회해 출력할 수 있다.
- **`Debugger`**(`StatementHook`으로 등록되는 콜백 객체)의 내부 상태:
  - `breakpoints_: std::set<int>` (줄 번호)
  - `watches_: std::vector<std::string>` (변수 이름)
  - `pendingStopDepth_: std::optional<int>` — `next` 명령이 "현재 문장과 같은 깊이로
    돌아올 때까지는 멈추지 않는다"를 구현하기 위한 현재 깊이 기준점(중첩 블록/함수 호출
    깊이를 Debugger가 직접 셀 수도 있고, `Environment::depth()`처럼 현재 스코프 개수를
    조회하는 보조 메서드를 하나 더 둬도 된다).
  - 훅이 호출될 때마다(즉 **모든 Statement 실행 전에 항상 호출됨**):
    1. `stmt->getLine()`으로 현재 줄을 구한다.
    2. **멈출지 판단**: `step` 모드면 항상 멈춘다. `next` 모드면 `pendingStopDepth_` 이하
       깊이로 돌아왔을 때만 멈춘다. `continue`/`run` 모드면 **`stmt->containsLine(line)`이
       `breakpoints_`의 어떤 값과 겹칠 때만 멈춘다** — 즉 "breakpoint 걸기"는 Executor
       쪽에 특별한 처리를 추가하는 게 아니라, **매 statement마다 실행되는 이 훅이 매번
       breakpoint 집합을 검사하는 것으로 충분**하며, 이는 "brekapoint까지 매 줄을 step
       하며 확인해 나가는 것"과 동작이 완전히 동일하다(요구사항 문구 그대로: "break point는
       step의 반복으로 실행").
    3. 멈추면: `watches_`의 각 이름을 `executor.environment().lookup(name)`으로 조회해
       출력하고, 표준 입력에서 한 줄 명령을 읽어 `step`/`next`/`break N`/`breakpoints`/
       `remove N`/`continue`/`watch X`/`unwatch X`/`watches`/`inspect`를 파싱해 상태를
       갱신한다(각 명령을 작은 `Command` 객체로 만들면 Command 패턴이 자연스럽게 적용됨,
       §10).

### 9.4 REPL 모드와의 관계

`RunPromptShell`은 변경하지 않는다(훅을 쓰지 않으므로 기존 `Executor` 그대로 동작).
`DebugMode`만 `Executor`를 생성한 뒤 `setStatementHook`으로 `Debugger`를 등록해 실행한다.

## 10. 새/변경 파일 요약

| Unit | 신규 파일 | 주요 변경 파일 |
|---|---|---|
| Tokenizer | — | `Token.h` (TokenType 추가: FUNC/RETURN/CLASS/THIS/ARRAY/IMPORT/ALIAS/INSTANCEOF/DOT/LEFT_BRACKET/RIGHT_BRACKET/COMMA) |
| Assembler | `SourceReaderInterface.h`, `FileSourceReader.h/.cpp` | `SyntaxTree.h` (신규 노드 — `MethodDeclareStatement` 포함, `AssignExpression` 대상 일반화, `IdentifierExpression::depth`, `SyntaxNode::containsLine`, 자식 포인터 const 완화), `Assembler.h/.cpp` (신규 문법 파싱 + postfix 체인 + import 재귀 컴파일, `SourceReaderInterface&` 의존성 추가) |
| Checker | `Resolver.h/.cpp`(선택), `ConstantFolder.h/.cpp`(선택) | `Checker.h/.cpp` (함수/클래스/import 의미 검사, Resolver·ConstantFolder 호출, `ExecuteInterface&` 의존성 추가), `CheckerInterface.h` |
| Executor | `InstanceValue.h`, `ArrayValue.h` | `Value.h/.cpp` (참조 타입 추가), `Environment.h/.cpp` (`lookupAt`/`assignAt`), `Executor.h/.cpp` (신규 노드 핸들러, ReturnSignal, StatementHook), `ExecuteInterface.h` (`evaluate`, `environment` 공개 메서드 추가) |
| Shell | `FileRunMode.h/.cpp`, `DebugMode.h/.cpp`, `Debugger.h/.cpp`, `CommandLineArgs.h/.cpp` | `main.cpp` (모드 분기, `SourceReaderInterface` 구현체 생성), `RunPromptShell.h/.cpp` (변경 없음 또는 import 관련 예외 처리 추가) |

프로젝트 파일(`CodeFab.vcxproj`/`.vcxproj.filters`)에는 위 신규 파일들을 각 폴더 필터에
맞춰 추가해야 한다.

## 11. 적용 가능한 디자인 패턴

- **Interpreter / Visitor(변형)** — `Executor`의 `type_index → handler` 테이블 디스패치가
  Visitor의 변형이다. 새 노드 타입도 `registerDefaultHandlers()`에 핸들러를 추가하는
  것만으로 확장된다.
- **Composite** — `SyntaxNode` 트리 자체가 Composite 패턴이며, `ConstantFolder`가 이를
  그대로 활용한다.
- **Command** — Debugger의 각 명령(`step`/`break`/`watch` 등)을 `Command` 객체로
  캡슐화하면 명령 추가/조합이 쉬워진다.
- **Strategy** — Assembler가 주입받는 `SourceReaderInterface`(내부적으로
  `TokenizeInterface`를 캡슐화한 실제 파일 시스템 구현 `FileSourceReader` vs.
  테스트용 인메모리 Fake), 그리고 정적 바인딩 성공/실패에 따라 `Environment` 조회가
  `*At`(정적)과 `lookup`(동적) 사이에서 갈리는 부분이 Strategy 패턴에 해당한다.
- **Facade** — `SourceReaderInterface`가 "파일 읽기 + 토큰화"라는 두 단계를 `read(path)`
  호출 하나로 감싸서, Assembler가 `TokenizeInterface`를 직접 알 필요 없이 바로
  `vector<Token>`을 받게 해준다.
- **Template Method(참고)** — `Checker::check()`가 SemanticAnalyzer → Resolver →
  ConstantFolder 순서로 고정된 절차를 밟는 구조가 Template Method적 성격을 갖는다.

## 12. 구현 순서 제안

1. **Token/SyntaxTree 확장** (§2.1, §2.2) — 다른 모든 작업의 전제조건. 기존 테스트가
   깨지지 않는지 먼저 확인한다.
2. **Value 확장** (§2.3) — `Function`/`Class`/`Instance`부터. `Array`/`Module`은 이후
   단계에서 점진적으로 추가 가능.
3. **Function** (§3) — 가장 단순한 새 실행 단위. `ReturnSignal` 메커니즘을 여기서 먼저
   검증한다.
4. **Class** (§4) — Function 인프라(호출, 스코프) 위에 필드/메서드/`this`/`init`을 얹는다.
5. **정적 배열** (§5) — 독립적이므로 아무 때나 넣어도 되지만, Function 인프라(호출 파싱
   체인) 완성 후가 자연스럽다.
6. **실행 전 최적화** (§6) — 1~5가 만든 새 노드 타입들이 Resolver/ConstantFolder 순회에서
   빠짐없이 처리되는지 함께 검증해야 하므로 마지막에 붙이는 편이 손이 덜 간다.
   `ExecuteInterface::evaluate` 추가는 Checker가 이를 의존하기 시작하는 시점에 맞춰 진행.
7. **Import** (§7) — 4-Unit이 안정된 뒤, Assembler가 자기 자신을 재귀 호출할 수 있는
   구조가 준비된 시점에 붙인다.
8. **공장 제어 쉘** (§9) — 전체 기능이 갖춰진 뒤 REPL 외 모드를 얹는 것이 자연스럽다
   (디버그 모드가 결국 모든 새 노드 타입을 다 실행해볼 수 있어야 하므로).

## 13. 리스크와 하위 호환성

- **`AssignExpression` 대상 일반화**(§2.2)는 기존에 `identifier`라는 이름으로 접근하던
  코드(Checker의 대입 검사, Executor의 대입 실행)를 모두 `target`의 실제 타입을
  `dynamic_cast`로 분기하도록 바꿔야 한다. 기존 "대입 좌변이 identifier가 아니면 파싱
  오류" 테스트는 "셋 중 하나가 아니면 오류"로 기대값을 갱신해야 한다.
- **노드 불변성 완화**(§6.2)는 `operator==`(값 비교) 자체에는 영향이 없지만, "생성 후
  트리 구조가 바뀔 수 있다"는 전제가 새로 생긴다. 상수 폴딩이 켜진 뒤에는
  `AssemblerTest`의 트리 동등성 비교가 "폴딩되지 않은 원본"과 다를 수 있으므로, 이런
  테스트는 최적화 이전 단계(Checker 호출 전)의 트리만 검사하도록 하거나 기대값을 폴딩
  결과로 갱신해야 한다.
- **`Assembler` 생성자 시그니처 변경**(`SourceReaderInterface&` 주입 — `Tokenizer`는
  더 이상 Assembler가 직접 받지 않고 `FileSourceReader` 내부에 캡슐화됨)과 **`Checker`
  생성자 시그니처 변경**(`ExecuteInterface&` 주입)은 `AssemblerInterface`/
  `CheckerInterface`를 통해 다형적으로 쓰는 `RunPromptShell`에는 영향이 없지만, 구체
  클래스를 직접 생성하는 `main.cpp`, `AssemblerTest.cpp`, `CheckerTest.cpp`의 생성자
  호출부는 함께 수정해야 한다. `main.cpp`는 이제 `Tokenizer` → `FileSourceReader
  (tokenizer)` → `Assembler(sourceReader)` 순서로 만들어야 한다.
- **`MethodDeclareStatement`를 `FunctionDeclareStatement`와 별도 타입으로 둔 결정**은
  Checker/Executor가 "이건 최상위 함수다"와 "이건 메서드다"를 노드 타입만으로 구분할 수
  있게 해주지만, 반대로 두 타입에 대해 각각 별도의 `dynamic_cast`/핸들러 등록이
  필요하다는 뜻이기도 하다 - Executor에서는 §3.3에서 제안한 공용 `invoke()` 헬퍼로
  중복을 줄인다.
- **`ExecuteInterface`에 `evaluate`/`environment` 추가**는 인터페이스 확장이므로, 이
  인터페이스를 구현하는 테스트용 Mock/Fake(`RunPromptShellTest.cpp` 등에 있을 수 있는
  `MockExecutor` 류)도 새 순수 가상 함수를 구현하도록 함께 갱신해야 컴파일된다.
- **Assembler ↔ 자기 재귀(import)**: 순환 import 검출용 `importStack_`은 인스턴스
  멤버이므로, `Assembler` 인스턴스를 여러 스레드에서 동시에 쓰는 시나리오는 없다고
  가정한다(현재 프로젝트는 단일 스레드 REPL/파일 실행이므로 문제없음).
- **성능/메모리**: 상수 폴딩으로 죽은 노드가 `SyntaxTree`에 남는 것, import를 여러 번
  하면 파일을 다시 읽고 다시 파싱하는 것 모두 이번 범위에서는 의도된 트레이드오프다
  (스크립트 하나의 실행 수명 동안만 유효하므로 실질적 문제가 되지 않음). 필요해지면
  "파일 경로 → 이미 컴파일된 `SyntaxTree`" 캐시를 `Assembler`에 추가하는 것으로 나중에
  최적화할 수 있다.
