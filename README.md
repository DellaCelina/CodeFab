# CodeFab
---
## Introduce
2026 CRS 과정 5차수 team3 CodeFab team project입니다.

CodeFab은 Custom Language를 실행하는 Interpreter이자, 그 언어를 즉시 실행해볼 수 있는
Prompt Shell(REPL)입니다.

# Ground Rule
팀명 : CommitGuard
팀명 의미: Git에 Commit되기 전, 코드의 품질을 지키는 파수꾼(Guard) 역할을 하자

- 퇴근 시간은 17:30, 점심 시간은 11:30~13:00, 쉬는 시간은 유동적으로
- Commit prefix: [fix], [test], [feat], [refactor]
- github Webhook은 discord 질문 탭, 대화는 일반 tab을 사용
- Code format은 Camel
- PR approve는 최소 2명
- branch 이름은 feature/... merge 후에는 PR 생성자가 삭제하는 것으로
- PR message는 Claude.md를 활용해서 commit 내용들 요약
- Merge 정책은 기본 merge 사용 (rebase, sqush 제외)
- Merge commit 이름은 auto merge 이름을 바꾸지 않음

# 협업 Rule
- 리뷰시 구체적 대안 코드를 제시해서 리뷰 진행하면 feedback 공유해서 진행한다.
- 리뷰 처음 진행이라 이모티콘 사용해서 유하게 해주는 것도 좋겠다.
- commit 시 코드 설명을 부연해주면 좋겠다. 변경점에 대한 설명을 해주면 리뷰어들이 빠른 확인이 된다.
- pr 에는 commit 설명으로, commit 시에는 리뷰어를 위한 변경점 정보른 남겨주는게 좋겠다.
  추가로 하루 1개의 칭찬 comment 를 남기면 좋겠다.
  rebase 후 merge 로 정책을 바꾸면 좋겠다. 해보고 불편하면 정책을 재검토하자.
- 설명 추가로 commit 부분 외에 실제 코드 변경 부분에도 "+" 아이콘을 이용해 부연설명을 할 수 있다.

## Naming Rule

전체 코드베이스를 관통하는 네이밍 규칙은 다음과 같습니다.

| 대상 | 규칙 | 예시 |
|---|---|---|
| 클래스/구조체 | PascalCase | `Tokenizer`, `Assembler`, `Checker`, `Executor`, `Environment`, `Scope`, `Value`, `RunPromptShell`, `SyntaxTree` |
| 파일명 | PascalCase (클래스명과 1:1 대응) | `Executor.h` / `Executor.cpp` |
| Unit의 추상 인터페이스 | `<Unit>Interface.h` 파일 안에 `<Unit>Interface` 추상 클래스를 정의 | `AssemblerInterface.h` → `AssemblerInterface` |
| Unit이 던지는 예외 | 정의부와 같은 `<Unit>Interface.h`에 `<Unit>Error` (`std::exception` 직접 상속, `std::format` 기반 생성자) 형태로 정의 | `CheckerInterface.h` → `CheckerError` |
| 테스트 파일 | `<대상>Test.cpp`, 세부 영역이 나뉘면 `<대상><영역>Test.cpp` | `AssemblerTest.cpp`, `ExecutorControlFlowTest.cpp` |
| 멤버 변수(private) | 트레일링 언더스코어 | `out_`, `environment_`, `scopes_`, `variables_` |
| 함수/메서드 | camelCase | `tokenize()`, `assemble()`, `check()`, `execute()`, `pushScope()` |

예외 클래스는 별도의 공용 에러 헤더에 모아두지 않고, 그 예외를 던지는 인터페이스가 정의된
헤더(`<Unit>Interface.h`)에 함께 둡니다. 이렇게 하면 어떤 인터페이스의 구현체가 어떤 예외를
던질 수 있는지 헤더 하나만 보고 알 수 있습니다.

## 사용 방법

Visual Studio에서 `CodeFab.slnx`를 열고 빌드하면 `x64/Debug/CodeFab.exe`가 생성됩니다.

- `_DEBUG` 빌드(Debug 구성)에서는 gtest/gmock 기반의 전체 유닛 테스트가 실행됩니다.
- Release 구성(또는 `_DEBUG`가 정의되지 않은 빌드)에서는 아래 CLI 모드 중 하나로 동작합니다.

```
CodeFab                  프롬프트(REPL) 모드로 시작합니다.
                          한 줄씩 입력받아 즉시 실행 결과를 보여줍니다.
CodeFab run <path>       파일 모드. <path>의 소스 파일 전체를 한 번에 실행하고 종료합니다.
CodeFab debug <path>     디버그 모드. (설계만 있고 아직 구현되지 않았습니다 - 실행하면
                          "이 모드는 아직 구현되지 않았습니다"를 출력하고 종료 코드 1을 반환)
CodeFab --help, -h       도움말을 출력합니다.
```

REPL 모드는 `>>> ` 프롬프트를 출력하고 한 줄씩 입력을 받습니다. 줄 끝을 `\`로 끝내면
다음 줄과 이어붙여서(`... ` 프롬프트로 표시) 여러 줄짜리 문장을 입력할 수 있고, 괄호나
문자열이 아직 닫히지 않은 경우도 자동으로 다음 줄을 이어받습니다. `exit`을 입력하면
종료합니다. 파일 모드는 파일 전체 내용을 `{ ... }`로 감싸 하나의 블록으로 실행하므로,
한 파일에 여러 top-level 문장을 그대로 나열해도 됩니다.

## CodeFab 언어 문법

아래 문법은 실제 구현(`Assembler.cpp`)을 기준으로 정리했습니다. 낮은 우선순위부터
높은 우선순위 순서입니다.

```
statement      -> printStmt | declareStmt | blockStmt | ifStmt | forStmt
                 | funcDeclStmt | classDeclStmt | returnStmt | importStmt | exprStmt
printStmt      -> PRINT expression(0) ';'
declareStmt    -> 'var' IDENTIFIER '=' expression(0) ';'
blockStmt      -> '{' statement* '}'
ifStmt         -> 'if' '(' expression(0) ')' statement ('else' statement)?
forStmt        -> 'for' '(' forInit ';' expression(0) ';' expression(0) ')' statement
forInit        -> declareStmt | expression(0) ';'
funcDeclStmt   -> 'Func' IDENTIFIER '(' params? ')' blockStmt
params         -> IDENTIFIER (',' IDENTIFIER)*
classDeclStmt  -> 'Class' IDENTIFIER (':' IDENTIFIER)? '{' methodDeclStmt* '}'
methodDeclStmt -> IDENTIFIER '(' params? ')' blockStmt        // Func 키워드 없음
returnStmt     -> 'return' expression(0)? ';'
importStmt     -> 'import' STRING 'alias' IDENTIFIER ';'
exprStmt       -> expression(0) ';'

expression(0)  -> assignment            // '='  (우결합, 최하위 우선순위)
                -> logicalOr            // 'or'
                -> logicalAnd           // 'and'
                -> equality             // '==' '!='
                -> comparison           // '<' '<=' '>' '>=' 'instanceof'
                -> term                 // '+' '-'
                -> factor               // '*' '/' '%'
                -> unary                // '-' '!' (전위, 우결합)
call           -> primary ( '(' arguments? ')' | '.' IDENTIFIER | '[' expression(0) ']' )*
arguments      -> expression(0) (',' expression(0))*
primary        -> NUMBER | STRING | 'true' | 'false' | 'This' | 'Super' | IDENTIFIER
                 | 'Array' '(' expression(0) ')'
                 | '(' expression(0) ')'
```

`instanceof`는 비교 연산자와 같은 우선순위 레벨에 있지만, 우변이 항상 클래스 이름
(식별자)이어야 하므로 재귀적으로 표현식을 파싱하지 않고 그 이름을 그대로 토큰으로
받습니다. `call`의 후위(postfix) 체인은 `add(1,2)`, `r.move(5)`, `arr[i]`, 그리고
`r.list[0]()`처럼 이들의 조합을 모두 좌결합으로 처리합니다.

### 데이터 타입

| 타입 | 리터럴/생성 방법 | 비고 |
|---|---|---|
| `Nil` | (선언하지 않은 값의 기본값, `Func`가 `return` 없이 끝났을 때) | `print`하면 `nil` |
| `Boolean` | `true`, `false` | |
| `Number` | `3`, `3.14` (내부적으로 모두 `double`) | 정수처럼 보이면 소수점 없이 출력(`5.0` → `5`) |
| `String` | `"text"` | `+`로 다른 문자열과 연결 가능 |
| `Function` | `Func` 선언 | 값으로 취급되진 않고 이름으로 호출만 가능 |
| `Class` | `Class` 선언 | 인스턴스 생성(`Robot()`)에 사용 |
| `Instance` | `Robot()`처럼 클래스를 호출 | 필드는 동적으로 추가 가능 |
| `Array` | `Array(n)` | 크기 고정, 초기값은 전부 `Nil` |
| `Module` | `import ... alias name;` | `name.member` 형태로 접근 |

### 변수와 출력

```
var a = 10;
var b = "hello";
print a + 5;
a = a + 1;          // 재대입
```

`var`는 항상 초깃값과 함께 선언해야 하고, 같은 스코프에서 이름이 중복되면 오류입니다.
초기화식에서 자기 자신(선언 중인 변수)을 참조할 수 없습니다(`var a = a;`는 오류).

### 연산자

| 분류 | 연산자 | 비고 |
|---|---|---|
| 산술 | `+` `-` `*` `/` `%` | `+`는 number+number(합) 또는 string+string(연결)만 허용, 나머지는 number만 |
| 비교 | `<` `<=` `>` `>=` | number만 허용 |
| 동등 | `==` `!=` | 타입/값 비교 |
| 논리 | `and` `or` `!` | `and`/`or`는 단락(short-circuit) 평가, truthy 기준(`nil`/`false`만 falsy) |
| 대입 | `=` | 좌변은 식별자, 필드(`r.x`), 배열 원소(`arr[i]`) 중 하나만 허용 |
| 타입 검사 | `instanceof` | `a instanceof ClassName` |

### 블록과 스코프

```
{
    var x = "inner";
    print x;
}
```

`{ }`는 새 블록 스코프를 열며, 안에서 선언한 변수는 블록을 벗어나면 사라집니다(바깥
스코프의 동일 이름 변수를 가리는 shadowing도 가능). `if`/`for` 문의 바디도 각각 자기
스코프를 갖습니다.

### 제어 흐름

```
if (조건) 문장;
if (조건) 문장; else 문장;

for (var i = 0; i < 3; i = i + 1) {
    print i;
}
```

`if`는 dangling-else를 가장 가까운 `if`에 결합시킵니다. `for`의 초기화절은 `var` 선언
또는 일반 expression 문 중 하나입니다.

### 함수 (Func)

```
Func add(a, b) {
    return a + b;
}
print add(3, 4);        // 7

Func fact(n) {
    if (n <= 1) { return 1; }
    return n * fact(n - 1);
}
print fact(5);           // 120 (재귀 지원)
```

- 파라미터는 값 전달(호출자의 변수에 영향 없음)입니다.
- `return;`(값 없음) 또는 `return`문이 전혀 없으면 결과는 `Nil`입니다.
- 함수 선언은 재귀 호출이 가능하도록 이름을 바디 실행 전에 먼저 등록합니다.
- 함수 밖에서 `return` 사용, 파라미터 이름 중복, 호출 시 인자 개수 불일치, 함수가
  아닌 값 호출은 모두 오류입니다(아래 오류 목록 참고).

### 클래스 (Class)

```
Class Robot {
    init(name) {
        This.name = name;
    }
    getName() {
        return This.name;
    }
    move(dist) {
        This.position = This.position + dist;
    }
}

var r = Robot("ABC");
r.position = 0;
r.move(5);
print r.getName();       // ABC
print r.position;        // 5
```

- 클래스 바디 안의 메서드는 `Func` 키워드 없이 `이름(params) { ... }` 형태로 선언합니다.
- 생성자는 이름이 관례적으로 `init`인 평범한 메서드입니다. `Robot("ABC")`처럼 클래스를
  호출하면 새 인스턴스를 만들고 `init`을 실행한 뒤 그 인스턴스를 반환합니다(반환값은
  버림). `init`이 없으면 인자 없이 인스턴스만 생성됩니다.
- 필드는 미리 선언할 필요가 없습니다 - `This.필드 = 값`으로 처음 대입하는 순간 생성되고,
  이미 있으면 갱신됩니다.
- `This`는 클래스 메서드 안에서만 사용할 수 있고, 현재 인스턴스를 가리킵니다.
- `init` 메서드는 값이 있는 `return`을 쓸 수 없습니다(`return;`은 허용 여부가 팀
  컨벤션으로 금지되어 있음 - 값 있는 `return`만 오류로 검사됨).

### 상속 (Super, `:`) — 문법 파싱만 구현, 의미 검사·실행은 미구현

```
Class Robot {
    move(dist) { print "move"; }
}
Class SpeedRobot : Robot {
    move(dist) {
        Super.move(dist);
        print "Speeeed!";
    }
}
```

`Class 자식 : 부모 { ... }` 문법과 `Super.method(...)`/`Super.field` 표현식은
Assembler가 이미 파싱할 수 있습니다. 하지만 Checker의 상속 의미 검사(자기 상속 금지,
클래스가 아닌 대상 상속 금지, `Super` 사용 위치 검사)와 Executor의 실행 규칙(메서드
오버라이딩, `superclass` 체인 탐색, `instanceof`의 부모 클래스 판정)은 아직 구현되어
있지 않습니다 - 상속을 실제로 실행하면 자식 클래스의 메서드 탐색이 부모까지 올라가지
않고, `Super` 관련 검사도 수행되지 않습니다(`IntegrationTest/DebugIntegrationTest.cpp`에
`DISABLED_` 접두사로 남겨진, 구현되면 통과해야 할 시나리오들이 있습니다).

### 정적 배열 (Array)

```
var arr = Array(3);      // [nil, nil, nil]
arr[0] = 10;
var i = 2;
arr[i - 1] = 7;           // arr[1] = 7
print arr[0];             // 10
```

- `Array(n)`은 크기가 `n`으로 고정된 배열을 만들고, 모든 원소를 `Nil`로 채웁니다.
- 크기는 반드시 number여야 하고, 인덱스도 반드시 number여야 하며, 범위(`[0, n)`)를
  벗어나면 오류입니다.

### 타입 검사 (instanceof)

```
Class Robot { }
var r = Robot();
print r instanceof Robot;   // true
print 1 instanceof Robot;   // false (인스턴스가 아니면 항상 false)
```

현재는 정확히 같은 클래스인지만 포인터 비교하며, 상속 관계(부모 클래스에 대해서도
`true`)는 아직 반영되지 않습니다(위 상속 항목 참고).

### 모듈 (import)

`math.cf`:
```
Func add(a, b) {
    return a + b;
}
```

메인 코드:
```
import "math.cf" alias sum;
print sum.add(1, 2);        // 3
```

- import 대상 파일에는 `var` 선언, `Func` 선언, `Class` 선언만 허용됩니다(그 외
  문장이 섞여 있으면 오류).
- import는 파일을 그 자리에서 재귀적으로 다시 파싱하며, 순환 import는 오류입니다.
- 같은 스코프에서 같은 alias로 두 번 import하거나, 상위 스코프에서 이미 import한
  이름을 하위 스코프에서 다시 import하면 오류입니다.
- 반복문(`for`) 바디 안에서는 import를 사용할 수 없습니다.

## 오류(Error) 종류와 메시지

CodeFab의 각 Unit은 자신만의 예외 타입을 던지며(모두 `std::exception`을 직접 상속),
Shell(REPL/파일 모드)은 이를 `catch (const std::exception&)` 한 번에 잡아 `e.what()`
메시지를 그대로 출력합니다. 오류 메시지는 현재 대부분 한글이며(영어로 통일하는 작업은
`TODO.md` "코드 정리 #1"에 남아 있음), `Checker`가 던지는 메시지에만 `[N번째 줄]`
줄 번호 접두사가 붙습니다.

### 1. Tokenizer — `AssemblyError` / `IncompleteInputError`

| 예외 | 발생 조건 | 메시지 예시 |
|---|---|---|
| `AssemblyError` | 알 수 없는 문자(정의되지 않은 기호)를 만남 | `[1번째 줄] 알 수 없는 문자: '?'` |
| `AssemblyError` | 문자열 리터럴이 끝까지 닫히지 않음 | `문자열이 종결되지 않았습니다.` |
| `IncompleteInputError` | (오류가 아님) REPL에서 문자열이 아직 안 닫힌 상태 - Shell이 다음 줄을 계속 입력받도록 하는 신호 | - |

### 2. Assembler — `AssemblerError`

문법(구문) 오류. 메시지에 `(near '토큰' at line N)` 접미사가 붙는 경우가 많습니다.

| 발생 조건 | 예시 입력 | 메시지 |
|---|---|---|
| 세미콜론 누락 | `print 1 + 2` | `Expect ';' after value.` |
| 닫는 괄호 누락 | `print (1 + 2 3);` | `Expect ')' after expression.` |
| 대입 좌변이 identifier/필드/배열 원소가 아님 | `a + b = 3;` | `Invalid assignment target.` |
| 식이 와야 할 자리에 다른 토큰 | `print * 5;` | `Expect expression.` |
| 블록이 안 닫힘 | `{ var x = 1;` | `Expect '}' after block.` |
| 순환 import | `a.cf`가 `b.cf`를, `b.cf`가 다시 `a.cf`를 import | `순환 import: '경로'` |
| import 대상 파일을 열 수 없음 | `import "missing.cf" alias m;` | `import 대상 파일을 열 수 없습니다: '경로' (사유)` |
| import 대상 파일에 선언 외의 문장이 있음 | import 대상에 `print 1;`만 있음 | `import 대상 파일에는 선언 외의 내용을 허용하지 않습니다: '경로'` |
| 이항 연산자 토큰이 처리되지 않음(내부 방어 코드) | (정상 사용 경로에서는 발생하지 않음) | `makeBinaryExpression: 처리되지 않은 연산자 토큰입니다.` |

### 3. Checker — `CheckerError`

의미(semantic) 오류. 모두 `[N번째 줄]` 접두사가 붙습니다.

| 발생 조건 | 예시 입력 | 메시지 |
|---|---|---|
| 초기화식에서 자기 참조 | `var a = a;` | `자신의 초기화식에서 지역변수를 읽을 수 없습니다.` |
| 같은 스코프에서 변수 중복 선언 | `var a = "hi"; var a = 3;` | `'a'에러: 이미 해당 변수는 현재 스코프에서 사용중입니다.` |
| 선언되지 않은 변수 참조 | `print notDefined;` | `'notDefined'에러: 선언되지 않은 변수입니다.` |
| 함수/메서드 밖에서 `return` | `return 5;` (최상위) | `함수(메서드) 밖에서 return을 사용할 수 없습니다.` |
| 파라미터 이름 중복 | `Func foo(a, a) { }` | `'foo'의 파라미터 이름 'a'이(가) 중복됩니다.` |
| 같은 스코프에서 함수/클래스 이름 중복 선언 | 같은 이름의 `Func`/`Class`를 두 번 선언 | `'이름'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다.` |
| 클래스 메서드 밖에서 `This` 사용 | `print This;` (최상위) | `클래스 메서드 밖에서 This를 사용할 수 없습니다.` |
| `init` 메서드에서 값 있는 `return` | `Class Robot { init() { return 5; } }` | `init 메서드는 값을 반환할 수 없습니다.` |
| 반복문(`for`) 안에서 import | `for (...) { import "x.cf" alias x; }` | `반복문(for) 안에서는 import를 사용할 수 없습니다.` |
| 같은 스코프에서 같은 alias로 중복 import | 같은 경로/alias를 두 번 import | `'alias'에러: 이미 해당 이름은 현재 스코프에서 사용중입니다.` |
| 상위 스코프에서 이미 사용 중인 alias를 하위에서 재import | - | `'alias'에러: 상위 스코프에서 이미 사용중인 이름입니다.` |

**상속 관련 검사(설계는 확정, 아직 구현 전)** — 구현되면 아래 메시지로 나올 예정입니다
(현재 코드에는 해당 검사가 없어 통과되어 버립니다).

| 발생 조건(예정) | 메시지(예정) |
|---|---|
| 자기 자신을 상속 | `'Robot' 클래스는 자기 자신을 상속할 수 없습니다.` |
| 클래스가 아닌 대상을 상속 | `'x'은(는) 클래스가 아니므로 상속할 수 없습니다.` |
| 클래스 메서드 밖에서 `Super` 사용 | `클래스 메서드 밖에서 Super를 사용할 수 없습니다.` |
| 부모 클래스가 없는 클래스 안에서 `Super` 사용 | `부모 클래스가 없는 클래스에서 Super를 사용할 수 없습니다.` |

### 4. Executor — `ExecutorError`

실행 중(런타임) 오류. 줄 번호를 담지 않으므로 접두사 없이 메시지만 출력됩니다.

| 발생 조건 | 예시 입력 | 메시지 |
|---|---|---|
| 산술/비교 연산자의 피연산자 타입 불일치 | `print 1 + "HI";` | `타입 오류: number + string` (연산자별로 `-`, `*`, `/`, `%`, `<`, `<=`, `>`, `>=`도 동일 형식) |
| 단항 `-`의 피연산자가 number가 아님 | `print -"FabCoding";` | `타입 오류: -string` |
| 0으로 나누기 | `print 3 / 0;`, `print 10 % 0;` | `0으로 나눌 수 없습니다` |
| 대입/참조 시 미정의 변수 (Checker가 대부분 선점하므로 실전에서는 드묾) | - | `'이름' 변수가 정의되지 않았습니다.` |
| 호출 인자 개수 불일치 | `Func oneArg(a) { } oneArg();` | `'oneArg' 호출에는 인자 1개가 필요합니다 (전달된 인자: 0개)` |
| 함수/클래스가 아닌 값을 호출 | `var x = 1; x();` | `호출할 수 없는 대상입니다.` |
| 존재하지 않는 필드 읽기 | `Class Empty { } print Empty().missing;` | `'missing' 필드가 존재하지 않습니다.` |
| 존재하지 않는 메서드 호출 | `Class Empty { } Empty().missing();` | `'missing' 메서드가 존재하지 않습니다.` |
| 인스턴스가 아닌 대상의 필드에 접근/대입 | `var x = "hello"; x.field = 1;` | `인스턴스가 아닌 대상에 필드를 대입했습니다.` (읽기는 `인스턴스가 아닌 대상의 필드에 접근했습니다.`) |
| 인스턴스가 아닌 대상의 메서드 호출 | `var x = 1; x.foo();` | `인스턴스가 아닌 대상의 메서드를 호출했습니다.` |
| 클래스 메서드 밖에서 `This` 사용(Checker가 대부분 선점) | - | `클래스 외부에서 This를 사용했습니다.` |
| 모듈에 없는 이름 접근 | `math.pi`인데 `pi`가 없음 | `모듈에 '이름'이(가) 없습니다.` |
| 모듈에 없는 함수 호출 | `sum.notExist();` | `모듈에 '이름' 함수가 없습니다.` |
| 배열 크기가 number가 아님 | `Array("hi");` | `배열의 사이즈는 반드시 number여야 합니다.` |
| 배열이 아닌 값 인덱싱 | `var x = 1; x[0];` | `index 접근은 오직 배열만 지원합니다.` |
| 인덱스가 number가 아님 | `arr["zero"];` | `인덱스는 반드시 숫자여야 합니다.` |
| 배열 인덱스가 범위를 벗어남 | `var arr = Array(3); arr[3];` | `배열 인덱스 범위를 벗어났습니다.` |
| `instanceof` 우변이 클래스가 아님 | `1 instanceof notAClass;` | `'notAClass'은(는) 클래스가 아닙니다.` |
| 지원하지 않는 대입 대상(내부 방어 코드) | (정상 파싱 경로에서는 도달하지 않음) | `아직 지원하지 않는 대입 대상입니다.` |

### 5. Shell(REPL/파일 모드) 자체 메시지

Shell은 위 예외들을 그대로 노출하는 것 외에, 아래 상황에서 자체 메시지를 출력합니다.

| 상황 | 메시지 |
|---|---|
| `check()`가 예외 없이 `false`를 반환(현재 구현은 항상 `true`를 반환하므로 실전에서는 도달하지 않음) | `코드 검사에 실패했습니다.` |
| 파일 모드에서 경로가 파일이 아님(디렉터리 등) | `path는 파일 1개(단일 파일)여야 합니다: <경로>` |
| 파일 모드에서 파일을 열 수 없음 | `파일을 열 수 없습니다: <경로>` |
| `debug` 모드 실행 시도(아직 미구현) | `이 모드는 아직 구현되지 않았습니다. --help로 사용 가능한 모드를 확인하세요.` |
| CLI 인자 오류(`run`/`debug`에 경로 누락, 알 수 없는 모드) | `run 모드는 실행할 파일 경로가 필요합니다. 사용법: CodeFab run <path>` 등 |

# 아키텍처

CodeFab은 소스코드를 입력받아 실행 결과를 만들어내는 파이프라인 구조의 인터프리터입니다.
소스코드가 공장의 컨베이어 벨트를 거치듯, 아래 4개의 Unit(컴포넌트)을 순서대로 통과합니다.

```
소스코드(string)
      │
      ▼
┌─────────────┐   Token 리스트   ┌─────────────┐   SyntaxTree   ┌─────────────┐   (검사 완료)   ┌─────────────┐
│  Tokenizer  │ ───────────────▶ │  Assembler  │ ─────────────▶ │   Checker   │ ──────────────▶ │  Executor   │
└─────────────┘                  └─────────────┘                └─────────────┘                  └─────────────┘
      ▲                                                                                                  │
      │                                                                                                  ▼
      └──────────────────────────────── RunPromptShell(REPL) ◀───────────────────────────── 실행 결과 / 출력
```

- **Tokenizer**: 소스코드 문자열을 의미 있는 최소 단위인 `Token`으로 분해한다.
- **Assembler**: `Token` 목록을 문법 규칙에 따라 가공하여 실행 가능한 트리 구조(`SyntaxTree`,
  `Statement`/`Expression` 노드)로 조립한다.
- **Checker**: 조립된 `SyntaxTree`를 실행하기 전에 DFS로 순회하며 의미상 오류(변수 중복 선언,
  선언 시 자기 참조 등)를 검사한다.
- **Executor**: 검사를 통과한 `SyntaxTree`를 DFS로 순회하며 실제로 실행하고, 변수 저장소
  (`Environment`/`Scope`)를 운용하며 결과값(`Value`)을 계산한다.
- **Shell**: 위 4개 Unit을 조합해 한 줄씩 입력받아 tokenize → assemble → check → execute
  파이프라인을 구동하는 Prompt Shell(REPL)이다. `main.cpp`가 각 Unit의 구체 클래스를
  생성해 `RunPromptShell`에 주입하는 composition root 역할을 한다.

## 폴더 구조

실제 소스 폴더 구조는 위 4개 Unit + Shell, 그리고 통합 테스트 스크립트 모음인
IntegrationTest를 반영합니다.

```
CodeFab/
├── Tokenizer/       # 소스코드 -> Token 분해
│   ├── Token.h              Token, TokenType 정의
│   ├── TokenizeInterface.h  TokenizeInterface 추상 클래스 + AssemblyError, IncompleteInputError
│   ├── Tokenizer.h / .cpp   TokenizeInterface 구현체
│   └── TokenizerTest.cpp
│
├── Assembler/       # Token -> SyntaxTree(문법 트리) 조립, import 파일 재귀 컴파일
│   ├── SyntaxTree.h            SyntaxNode/SyntaxTree 및 모든 Statement/Expression 노드 정의
│   ├── AssemblerInterface.h    AssemblerInterface 추상 클래스 + AssemblerError
│   ├── Assembler.h / .cpp      AssemblerInterface 구현체 (재귀 하향 파서)
│   ├── SourceReaderInterface.h import 대상 파일을 읽어 토큰화하는 추상 클래스
│   ├── FileSourceReader.h / .cpp  SourceReaderInterface의 실제 파일 시스템 구현체
│   └── AssemblerTest.cpp
│
├── Checker/         # SyntaxTree 실행 전 의미 오류 검사 및 실행 전 최적화
│   ├── CheckerInterface.h    CheckerInterface 추상 클래스 + CheckerError
│   ├── Checker.h / .cpp      CheckerInterface 구현체 (DFS 기반 의미 분석, 정적 바인딩)
│   ├── OptimizerInterface.h  OptimizerInterface 추상 클래스
│   ├── Optimizer.h / .cpp    OptimizerInterface 구현체 (상수 연산 폴딩)
│   ├── CheckerTest.cpp, OptimizerTest.cpp
│   └── README.md
│
├── Executor/        # SyntaxTree 실행
│   ├── ExecuteInterface.h    ExecuteInterface 추상 클래스 + ExecutorError
│   ├── Executor.h / .cpp     ExecuteInterface 구현체 (DFS 기반 트리 실행)
│   ├── Environment.h / .cpp  Scope 스택 관리 (변수 정의/대입/조회, 정적 바인딩 조회)
│   ├── Scope.h / .cpp        하나의 블록 스코프(이름 -> Value 테이블)
│   ├── Value.h / .cpp        런타임 값(Nil/Boolean/Number/String/Function/Class/Instance/Array/Module)
│   ├── ArrayValue.h          정적 배열 런타임 값
│   ├── InstanceValue.h       클래스 인스턴스 런타임 값
│   ├── ExecutorTest.cpp, ExecutorVariableTest.cpp, ExecutorControlFlowTest.cpp,
│   │   ExecutorFunctionTest.cpp, ExecutorClassTest.cpp, ExecutorArrayTest.cpp,
│   │   ExecutorImportTest.cpp, EnvironmentTest.cpp
│   └── README.md
│
├── Shell/           # 4개 Unit을 조합하는 REPL/파일/디버그 모드와 진입점
│   ├── RunPromptShell.h / .cpp  4개 *Interface에만 의존하는 REPL 루프
│   ├── FileRunMode.h / .cpp     소스 파일을 한 번에 읽어 실행하는 파일 모드
│   ├── DebugMode.h / .cpp       Stmt 단위 stepping을 지원하는 디버그 모드
│   ├── Debugger.h / .cpp        디버그 모드의 명령 처리기(step/break/watch 등)
│   ├── CommandLineArgs.h / .cpp argv 파싱으로 실행 모드를 선택
│   ├── RunPromptShellTest.cpp, FileRunModeTest.cpp, DebugModeTest.cpp,
│   │   DebuggerTest.cpp, CommandLineArgsTest.cpp
│   └── main.cpp                 각 Unit의 구체 클래스를 생성해 주입하는 composition root
│
└── IntegrationTest/ # 4-Unit 파이프라인 전체를 검증하는 통합 테스트 스크립트 모음
    ├── 01_arithmetic_precedence.txt ~ 15_error_division_by_zero.txt  기능별/오류별 스크립트
    ├── DebugIntegrationTest.cpp
    └── README.md
```

각 폴더는 하나의 Unit(책임)에 대응하며, 폴더 간 의존은 아래 방향으로만 흐릅니다.

```
Tokenizer ──▶ Assembler ──┬─▶ Checker  ──┐
                           └─▶ Executor ─┼─▶ Shell ──▶ main
                                         ┘
```

- `Assembler`는 `Tokenizer`의 `Token.h`만 참조한다.
- `Checker`, `Executor`는 `Assembler`의 `SyntaxTree.h`만 참조한다 (서로를 참조하지 않는다).
- `Shell`은 4개 Unit의 구체 클래스가 아니라 `*Interface.h` 4개(추상 클래스)에만 의존한다.
  구체 클래스(`Tokenizer`, `Assembler`, `Checker`, `Executor`)와의 결합은 `main.cpp`에서만
  이루어진다.

