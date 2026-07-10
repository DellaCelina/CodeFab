# CodeFab
---
## Introduce
2026 CRS 과정 5차수 team3 CodeFab team project입니다.

CodeFab은 Custom Language를 실행하는 Interpreter이자, 그 언어를 즉시 실행해볼 수 있는
Prompt Shell(REPL)입니다.

## 3~4일차 추가 기능 미션 체크리스트

`3일차_CodeFab Interpreter.pdf`(기능 추가 요청)에서 요구한 항목들과 실제 구현 여부입니다.

| 분류 | 기능 | 완료 |
|---|---|---|
| function | 함수 선언 (`Func add(a, b) { ... }`) | ✅ |
| function | 함수 호출 / 매개변수 전달 | ✅ |
| function | `return` 처리(값 없는 `return`은 `Nil`, `ret = add(1,2)`로 반환값 수신) | ✅ |
| function | 재귀 호출 | ✅ |
| function 오류 검사 | 함수 외부에서 `return` 사용 | ✅ |
| function 오류 검사 | 파라미터 이름 중복 | ✅ |
| function 오류 검사 | 함수가 아닌 대상 호출 | ✅ |
| function 오류 검사 | 인자 개수 불일치 | ✅ |
| class | 클래스 선언 / 인스턴스 생성 | ✅ |
| class | 필드 동적 읽기/쓰기/갱신, 존재하지 않는 필드 읽기 시 런타임 오류 | ✅ |
| class | 메서드 선언/호출, `This`로 필드 접근, 메서드 내부에서 다른 메서드 호출 | ✅ |
| class | 생성자(`init`), 생성 시 인자 전달, `init`의 값 있는 `return` 금지 | ✅ |
| class | 상속(`Class A : B`), 메서드 상속/오버라이딩, `Super` 호출 | ✅ |
| class | 타입 검사 연산자 `instanceof`(부모 클래스에 대해서도 `true`) | ✅ |
| class 오류 검사 | 클래스 외부 `This` 사용 / `init`의 값 있는 `return` | ✅ |
| class 오류 검사 | 자기 자신 상속 / 클래스가 아닌 대상 상속 | ✅ |
| class 오류 검사 | 클래스 외부 `Super` 사용 / 부모 없는 클래스에서 `Super` 사용 | ✅ |
| class 오류 검사 | 인스턴스가 아닌 대상의 필드 접근, 존재하지 않는 필드/메서드 접근 | ✅ |
| 정적 배열 | 고정 크기 배열 생성(`Array(n)`), 인덱스 읽기/쓰기 | ✅ |
| 정적 배열 | 런타임 오류(범위 초과, 인덱스/크기 타입 오류, 배열이 아닌 대상 인덱싱) | ✅ |
| 실행 전 최적화 | 지역 변수 정적 바인딩(Checker가 `depth` 계산 → Executor `O(1)` 접근) | ✅ |
| 실행 전 최적화 | 상수 연산 폴딩(리터럴 상수식을 미리 계산해 리터럴로 치환), `check()` 이후 `execute()` 이전에 실행 파이프라인(REPL/파일/디버그 모드)에 연결 | ✅ |
| 실행 전 최적화 | Test Double을 이용한 최적화 여부 검증 | ✅ |
| import | Library Import(`import "path" alias name;`) | ✅ |
| import | 반복문 내부 import 금지, 선언 외 문장 포함 시 오류 | ✅ |
| import | 파일 경로는 문자열 리터럴만 허용, 순환 import 오류 | ✅ |
| import | 같은/상위 스코프에서의 중복·재import 금지 | ✅ |
| 공장 제어 쉘 | 프롬프트 모드(REPL) | ✅ |
| 공장 제어 쉘 | 파일 모드(`run <path>`, 파일 없음/런타임 오류 시 줄 번호와 함께 오류 후 종료) | ✅ |
| 공장 제어 쉘 | 디버그 모드 stepping(`step`/`next`/`break`/`breakpoints`/`remove`/`continue`) | ✅ |
| 공장 제어 쉘 | 디버그 모드 watch(`watch`/`unwatch`/`watches`/`inspect`) | ✅ |

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
CodeFab debug <path>     디버그 모드. <path>의 소스 파일을 문장 단위로 멈춰가며
                          (breakpoint/step/watch) 실행합니다.
CodeFab --help, -h       도움말을 출력합니다.
```

- REPL Mode (인터프리터 모드) 제약 사항
 `>>> ` 프롬프트를 출력하고 한 줄씩 입력을 받습니다. 
 여러 줄짜리 문장을 입력시에는 줄 끝에 `\`을 붙여서 입력해야 합니다. (`... ` 프롬프트로 표시)(그 외의
경우 문자열/괄호가 닫히지 않았어도 그 줄까지만 즉시 실행을 시도하고 오류로 보고합니다).
소문자 `exit`을 입력하면 종료합니다. 
- RunFile Mode (파일 모드) 제약 사항
실행시 내부적으로 파일 전체 내용을 `{ ... }`로 감싸 하나의 블록으로 실행하므로, 한 파일에 여러 top-level 문장을 그대로 나열해도 됩니다.
- Debug Mode (디버그 모드) 제약 사항
class, func 정의 부분도 단위 실행시 표시됩니다. 모든 명령은 소문자로 사용해야 합니다.

### 디버그 모드 명령어

`CodeFab debug <path>`로 실행하면 첫 번째 문장 앞에서 자동으로 멈추고, 이후 아래 명령을
입력받습니다. 아무 명령도 주지 않고 입력이 끝나면(EOF) 나머지를 그대로 끝까지 실행합니다.

| 명령 | 동작 |
|---|---|
| `step` | 다음 문장 하나를 실행하고 다시 멈춤(문장 단위로 한 줄씩 진행) |
| `next` | 현재 문장을 통째로 실행하고, 같거나 더 얕은 깊이로 돌아왔을 때 멈춤(함수 호출 내부로는 들어가지 않음) |
| `continue` | 다음 브레이크포인트까지(없으면 끝까지) 실행 |
| `break <line>` | `<line>`에 브레이크포인트 설정 |
| `remove <line>` | `<line>`의 브레이크포인트 해제 |
| `breakpoints` | 현재 설정된 브레이크포인트 목록 출력 |
| `watch <name>` | 변수 `<name>`을 watch 목록에 추가하고 즉시 현재 값 출력(이후 멈출 때마다 값 표시) |
| `unwatch <name>` | `<name>`을 watch 목록에서 제거 |
| `watches` | 현재 watch 중인 변수들의 값 출력 |
| `inspect` | 현재 스코프의 지역 변수와 전역 변수를 이름/값/타입과 함께 모두 출력 |

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

### 상속 (Super, `:`)

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

var r = SpeedRobot();
r.move(5);
// move
// Speeeed!
```

`Class 자식 : 부모 { ... }` 문법으로 상속을 선언하고, 자식 클래스에서 부모와 같은 이름의
메서드를 다시 선언하면 오버라이딩됩니다(자식 것이 먼저 탐색되어 우선함). `Super.method(...)`
로 부모 클래스의 메서드를 명시적으로 호출할 수 있고, `Super.field`로 부모가 정의한 필드에도
접근할 수 있습니다.

| 제약 | 발생 조건 |
|---|---|
| 자기 자신 상속 금지 | `Class Robot : Robot { }` |
| 클래스가 아닌 대상 상속 금지 | `var x = 1; Class Robot : x { }` |
| `Super`는 클래스 메서드 안에서만 사용 가능 | 최상위 코드나 함수 안에서 `Super` 사용 |
| `Super`는 부모 클래스가 있는 클래스에서만 사용 가능 | 부모 없는 클래스의 메서드 안에서 `Super` 사용 |

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
Class SpeedRobot : Robot { }

var r = SpeedRobot();
print r instanceof SpeedRobot;   // true  (자기 자신의 클래스)
print r instanceof Robot;        // true  (부모 클래스도 인정)
print 1 instanceof Robot;        // false (인스턴스가 아니면 항상 false)
```

인스턴스가 속한 클래스부터 상속 체인을 따라 올라가며 대상 클래스와 일치하는지 확인하므로,
자기 자신의 클래스뿐 아니라 조상 클래스에 대해서도 `true`를 반환합니다.

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
Shell(REPL/파일/디버그 모드)은 이를 `catch (const std::exception&)` 한 번에 잡아
`e.what()` 메시지를 그대로 출력합니다. `Tokenizer`/`Assembler`/`Checker`/`Executor`가
던지는 메시지는 모두 영어이며, 대부분 `[line N]` 형식의 줄 번호를 포함합니다(정상적인
사용 경로에서는 발생하지 않는 내부 방어 코드 메시지만 줄 번호가 없습니다). CLI 인자
파싱 오류(Shell)만 한글 메시지를 그대로 사용합니다.

### 1. Tokenizer — `AssemblyError`

| 발생 조건 | 메시지 예시 |
|---|---|
| 알 수 없는 문자(정의되지 않은 기호)를 만남 | `[line 1] unknown character: '?'` |
| 문자열 리터럴이 끝까지 닫히지 않음 | `[line 1] unterminated string literal.` |

REPL에서 여러 줄짜리 문장을 입력하는 방법은 줄 끝의 `\`뿐입니다(위 "사용 방법" 참고) —
문자열/괄호가 닫히지 않은 채로 입력을 마치면 그 시점까지의 내용으로 즉시 tokenize를
시도하고, 위 오류를 그대로 보고합니다.

### 2. Assembler — `AssemblerError`

문법(구문) 오류. 파서가 특정 토큰에서 실패한 경우 `(near '토큰')` 접미사가 붙습니다.

| 발생 조건 | 예시 입력 | 메시지 |
|---|---|---|
| 세미콜론 누락 | `print 1 + 2` | `[line 1] Expect ';' after value. (near ...)` |
| 닫는 괄호 누락 | `print (1 + 2 3);` | `Expect ')' after expression.` |
| 대입 좌변이 identifier/필드/배열 원소가 아님 | `a + b = 3;` | `Invalid assignment target.` |
| 식이 와야 할 자리에 다른 토큰 | `print * 5;` | `Expect expression.` |
| 블록이 안 닫힘 | `{ var x = 1;` | `Expect '}' after block.` |
| 클래스 선언에서 `:` 다음에 부모 클래스 이름 누락 | `Class A : { }` | `Expect superclass name after ':'.` |
| 순환 import | `a.cf`가 `b.cf`를, `b.cf`가 다시 `a.cf`를 import | `[line N] circular import: '경로'` |
| import 대상 파일을 열 수 없음 | `import "missing.cf" alias m;` | `[line N] cannot open import file: '경로' (사유)` |
| import 대상 파일에 선언 외의 문장이 있음 | import 대상에 `print 1;`만 있음 | `[line N] import file may only contain declarations: '경로'` |
| 이항 연산자 토큰이 처리되지 않음(내부 방어 코드) | (정상 사용 경로에서는 발생하지 않음) | `unhandled binary operator token.` |

### 3. Checker — `CheckerError`

의미(semantic) 오류. 모두 `[line N]` 접두사가 붙습니다.

| 발생 조건 | 예시 입력 | 메시지 |
|---|---|---|
| 초기화식에서 자기 참조 | `var a = a;` | `cannot read local variable in its own initializer.` |
| 선언되지 않은 변수 참조 | `print notDefined;` | `'notDefined' is not declared.` |
| 같은 스코프에서 이름 중복 선언(변수/함수/클래스 공통) | `var a = "hi"; var a = 3;` | `'a' is already declared in this scope.` |
| 파라미터 이름 중복 | `Func foo(a, a) { }` | `duplicate parameter name 'a' in 'foo'.` |
| 함수/메서드 밖에서 `return` | `return 5;` (최상위) | `cannot use 'return' outside a function.` |
| `init` 메서드에서 값 있는 `return` | `Class Robot { init() { return 5; } }` | `'init' method cannot return a value.` |
| 클래스 메서드 밖에서 `This` 사용 | `print This;` (최상위) | `cannot use 'This' outside a class method.` |
| 클래스 메서드 밖에서 `Super` 사용 | `print Super;` (최상위) | `cannot use 'Super' outside a class method.` |
| 부모 클래스가 없는 클래스에서 `Super` 사용 | `Class Robot { m() { Super.m(); } }` | `cannot use 'Super' in a class with no superclass.` |
| 자기 자신을 상속 | `Class Robot : Robot { }` | `class 'Robot' cannot inherit from itself.` |
| 클래스가 아닌 대상을 상속 | `var x = 1; Class Robot : x { }` | `'x' is not a class and cannot be used as a superclass.` |
| 반복문(`for`) 안에서 import | `for (...) { import "x.cf" alias x; }` | `cannot use 'import' inside a for loop.` |
| 같은 스코프에서 같은 alias로 중복 import | 같은 경로/alias를 두 번 import | `'alias' is already declared in this scope.` |
| 상위 스코프에서 이미 사용 중인 alias를 하위에서 재import | — | `'alias' is already declared in an upper scope.` |

### 4. Executor — `ExecutorError`

실행 중(런타임) 오류. 클래스/모듈/배열 관련 오류는 각각 `ClassRuntime`, `ModuleRuntime`,
`ArrayRuntime`(모두 `Executor/`에 위치)이 던집니다.

| 발생 조건 | 예시 입력 | 메시지 |
|---|---|---|
| 산술/비교 연산자의 피연산자 타입 불일치 | `print 1 + "HI";` | `[line N] type error: number + string` (`-`, `*`, `/`, `%`, `<`, `<=`, `>`, `>=`도 동일 형식) |
| 단항 `-`의 피연산자가 number가 아님 | `print -"FabCoding";` | `[line N] type error: unary '-' requires a number, got string.` |
| 0으로 나누기 | `print 3 / 0;`, `print 10 % 0;` | `[line N] division by zero.` |
| 대입/참조 시 미정의 변수(Checker가 대부분 선점) | — | `[line N] '이름' is not defined.` |
| 호출 인자 개수 불일치 | `Func oneArg(a) { } oneArg();` | `[line N] 'oneArg' expects 1 argument(s) but got 0.` |
| 함수/클래스가 아닌 값을 호출 | `var x = 1; x();` | `[line N] callee is not callable.` |
| 존재하지 않는 필드 읽기 | `Class Empty { } print Empty().missing;` | `[line N] field 'missing' does not exist.` |
| 존재하지 않는 메서드 호출 | `Class Empty { } Empty().missing();` | `[line N] method 'missing' does not exist.` |
| 부모 클래스에도 없는 메서드를 `Super`로 호출 | `Super.missing();` | `[line N] method 'missing' does not exist in superclass.` |
| 인스턴스가 아닌 대상의 필드에 접근/대입 | `var x = "hi"; x.field = 1;` | `[line N] cannot access field on a non-instance.` (대입은 `cannot assign field to a non-instance.`) |
| 인스턴스가 아닌 대상의 메서드 호출 | `var x = 1; x.foo();` | `[line N] cannot call method on a non-instance.` |
| 클래스 메서드 밖에서 `This`/`Super` 사용(Checker가 대부분 선점) | — | `[line N] cannot use 'This' outside a class method.` / `cannot use 'Super' outside a class method.` |
| 모듈에 없는 이름 접근 | `math.pi`인데 `pi`가 없음 | `[line N] 'pi' is not defined in module.` |
| 모듈의 함수가 아닌 멤버 호출 | `sum.notExist();` | `[line N] 'notExist' is not a function in module.` |
| 배열 크기가 number가 아님 | `Array("hi");` | `[line N] array size must be a number.` |
| 배열이 아닌 값 인덱싱 | `var x = 1; x[0];` | `[line N] index access is only supported on arrays.` |
| 인덱스가 number가 아님 | `arr["zero"];` | `[line N] array index must be a number.` |
| 배열 인덱스가 범위를 벗어남 | `var arr = Array(3); arr[3];` | `[line N] array index out of bounds.` |
| `instanceof` 우변이 클래스가 아님 | `1 instanceof notAClass;` | `[line N] 'notAClass' is not a class.` |
| 지원하지 않는 대입 대상(내부 방어 코드) | (정상 파싱 경로에서는 도달하지 않음) | `[line N] invalid assignment target.` |

### 5. Shell(REPL/파일/디버그 모드) 자체 메시지

Shell은 위 예외들을 그대로 노출하는 것 외에, 아래 상황에서 자체 메시지를 출력합니다.

| 상황 | 메시지 |
|---|---|
| 파일/디버그 모드에서 경로가 파일이 아님(디렉터리 등) | `Error: path must be a single file: <경로>` |
| 파일/디버그 모드에서 파일을 열 수 없음 | `Error: cannot open file: <경로>` |
| CLI 인자 오류(`run`/`debug`에 경로 누락) | `run 모드는 실행할 파일 경로가 필요합니다. 사용법: CodeFab run <path>` (`debug`도 동일한 형식) |
| CLI 인자 오류(알 수 없는 모드) | `알 수 없는 모드입니다: '<모드>' (run, debug 중 하나를 사용하세요. ...)` |

# 아키텍처

CodeFab은 소스코드를 입력받아 실행 결과를 만들어내는 파이프라인 구조의 인터프리터입니다.
소스코드가 공장의 컨베이어 벨트를 거치듯, 아래 5단계를 순서대로 통과합니다(`Optimizer`는
`Checker`와 같은 `Checker/` 폴더에 속하지만, 파이프라인상으로는 검사가 끝난 뒤 실행 전에
한 번 더 거치는 별도 단계입니다).

```
소스코드(string)
      │
      ▼
┌─────────────┐  Token 리스트  ┌─────────────┐  SyntaxTree  ┌─────────────┐ (검사 완료) ┌─────────────┐ (최적화 완료) ┌─────────────┐
│  Tokenizer  │ ─────────────▶ │  Assembler  │ ───────────▶ │   Checker   │ ──────────▶ │  Optimizer  │ ────────────▶ │  Executor   │
└─────────────┘                └─────────────┘              └─────────────┘             └─────────────┘               └─────────────┘
      ▲                                                                                                                       │
      │                                                                                                                       ▼
      └────────────────────────────────────────── RunPromptShell(REPL) ◀───────────────────────────────────────────── 실행 결과 / 출력
```

- **Tokenizer**: 소스코드 문자열을 의미 있는 최소 단위인 `Token`으로 분해한다.
- **Assembler**: `Token` 목록을 문법 규칙에 따라 가공하여 실행 가능한 트리 구조(`SyntaxTree`,
  `Statement`/`Expression` 노드)로 조립한다.
- **Checker**: 조립된 `SyntaxTree`를 실행하기 전에 DFS로 순회하며 의미상 오류(변수 중복 선언,
  선언 시 자기 참조 등)를 검사하고, 지역 변수 참조에 정적 바인딩 거리(`depth`)를 채운다.
- **Optimizer**: `Checker`를 통과한 `SyntaxTree`에서 리터럴 상수식을 미리 계산해 리터럴
  노드로 치환한다(상수 폴딩). `Checker`와 책임이 분리되어 있지만, 항상 `check()` 성공
  직후 `execute()` 이전에 호출된다.
- **Executor**: `Checker`/`Optimizer`를 통과한 `SyntaxTree`를 DFS로 순회하며 실제로
  실행하고, 변수 저장소(`Environment`/`Scope`)를 운용하며 결과값(`Value`)을 계산한다.
  클래스/모듈/배열 관련 실행 로직은 `ClassRuntime`/`ModuleRuntime`/`ArrayRuntime`으로
  분리되어 있다.
- **Shell**: 위 4개 Unit(+`Optimizer`)을 조합해 한 줄씩 입력받아
  tokenize → assemble → check → optimize → execute 파이프라인을 구동하는 Prompt
  Shell(REPL)이다. `main.cpp`가 각 Unit의 구체 클래스를 생성해 `RunPromptShell`에
  주입하는 composition root 역할을 한다.

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
├── Checker/         # SyntaxTree 실행 전 의미 오류 검사 + 실행 전 최적화
│   ├── CheckerInterface.h    CheckerInterface 추상 클래스 + CheckerError
│   ├── Checker.h / .cpp      CheckerInterface 구현체 (DFS 기반 의미 분석, 정적 바인딩)
│   ├── OptimizerInterface.h  OptimizerInterface 추상 클래스
│   ├── Optimizer.h / .cpp    OptimizerInterface 구현체 (상수 연산 폴딩). `check()` 성공
│   │                         직후 `execute()` 이전에 Shell(main.cpp)이 호출한다
│   ├── CheckerTest.cpp, OptimizerTest.cpp
│   └── README.md
│
├── Executor/        # SyntaxTree 실행
│   ├── ExecuteInterface.h    ExecuteInterface 추상 클래스 + ExecutorError
│   ├── Executor.h / .cpp     ExecuteInterface 구현체 (DFS 기반 트리 실행)
│   ├── ClassRuntime.h / .cpp   클래스 인스턴스화, 메서드 탐색/상속 체인, instanceof 판정
│   ├── ModuleRuntime.h / .cpp  import된 모듈의 스코프 관리와 멤버 호출
│   ├── ArrayRuntime.h / .cpp   정적 배열 생성과 인덱스 접근
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
├── Shell/           # 4개 Unit + Optimizer를 조합하는 REPL/파일/디버그 모드와 진입점
│   ├── RunPromptShell.h / .cpp  5개 *Interface에만 의존하는 REPL 루프
│   ├── FileRunMode.h / .cpp     소스 파일을 한 번에 읽어 실행하는 파일 모드
│   ├── DebugMode.h / .cpp       문장 단위 stepping을 지원하는 디버그 모드
│   ├── Debugger.h / .cpp        디버그 모드의 명령 처리기(step/next/continue/break/watch/inspect)
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
Tokenizer ──▶ Assembler ──┬─▶ Checker(+Optimizer) ──┐
                           └─▶ Executor ─────────────┼─▶ Shell ──▶ main
                                                      ┘
```

- `Assembler`는 `Tokenizer`의 `Token.h`만 참조한다.
- `Checker`, `Executor`는 `Assembler`의 `SyntaxTree.h`만 참조한다 (서로를 참조하지 않는다).
  `Optimizer`(`Checker/` 폴더 소속)는 `Executor`의 `ExecuteInterface.h`(`evaluate()` 호출용)에
  의존한다.
- `Shell`은 4개 Unit의 구체 클래스가 아니라 `*Interface.h` 5개(`TokenizeInterface`/
  `AssemblerInterface`/`CheckerInterface`/`OptimizerInterface`/`ExecuteInterface`,
  추상 클래스)에만 의존한다. 구체 클래스(`Tokenizer`, `Assembler`, `Checker`, `Optimizer`,
  `Executor`)와의 결합은 `main.cpp`에서만 이루어진다.

