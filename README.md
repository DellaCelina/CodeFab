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
- Release 구성(또는 `_DEBUG`가 정의되지 않은 빌드)에서는 Prompt Shell(REPL)이 실행되어,
  표준 입력으로 한 줄씩 CodeFab 언어 코드를 입력하고 결과를 표준 출력으로 확인할 수 있습니다.
