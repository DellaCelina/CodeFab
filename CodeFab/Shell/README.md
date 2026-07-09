# Shell

Tokenizer/Assembler/Checker/Executor 4개 Unit과 `Optimizer`를 조합해 CodeFab을 실제로
실행 가능한 프로그램(`CodeFab.exe`)으로 만드는 Unit입니다. 실행 모드(REPL/파일/디버그)를
선택하는 CLI 파싱, 각 모드별 파이프라인 구동, `main.cpp`(composition root)를 포함합니다.

## 책임

- `CommandLineArgs::parse(argc, argv)`로 `argv`를 실행 모드(`Repl`/`Run`/`Debug`/`Help`)와
  경로로 해석한다.
- 각 모드에서 4개 Unit + `Optimizer`의 `*Interface`만 사용해
  `tokenize → assemble → check → optimize → execute` 파이프라인을 구동한다(구체 클래스와의
  결합은 `main.cpp`에만 존재).
- 파이프라인 중 던져진 예외(`AssemblyError`/`AssemblerError`/`CheckerError`/
  `ExecutorError`)를 `catch (const std::exception&)` 한 번에 잡아 `e.what()`을 그대로
  출력한다 — Shell 자신은 오류 메시지를 새로 만들지 않는다(단, 파일을 열 수 없는 경우
  등 파이프라인 이전 단계의 오류는 Shell이 직접 메시지를 낸다).
- 디버그 모드에서는 `Executor::setStatementHook`에 `Debugger`를 등록해 문장(Statement)
  단위로 실행을 멈추고 사용자 명령(step/next/continue/break/watch/inspect)을 처리한다.

## 파일 구성

| 파일 | 역할 |
|---|---|
| `CommandLineArgs.h` / `.cpp` | `argv` → `ShellMode`(`Repl`/`Run`/`Debug`/`Help`) + `path` 파싱, `usageText()` |
| `RunPromptShell.h` / `.cpp` | 프롬프트(REPL) 모드. 한 줄씩 입력받아 파이프라인을 구동하고, 세션 종료 전까지 `Environment`/`SyntaxTree` 소유권을 유지 |
| `FileRunMode.h` / `.cpp` | 파일 모드. 파일 전체를 `"{" + 내용 + "}"`로 감싸 한 번에 실행 |
| `DebugMode.h` / `.cpp` | 디버그 모드. `FileRunMode`와 같은 파이프라인에 `Debugger`를 훅으로 붙여 실행 |
| `Debugger.h` / `.cpp` | 디버그 모드의 명령 처리기 — 브레이크포인트/watch 상태를 들고 `step`/`next`/`continue`/`break`/`remove`/`breakpoints`/`watch`/`unwatch`/`watches`/`inspect` 명령을 해석 |
| `main.cpp` | 4개 Unit + `Optimizer`의 구체 클래스(`Tokenizer`/`Assembler`/`Checker`/`Optimizer`/`Executor`)를 생성해 위 모드 클래스들에 주입하는 composition root. `_DEBUG` 빌드에서는 대신 gtest를 실행 |

## 핵심 설계

- ***Interface 의존만 허용**: `RunPromptShell`/`FileRunMode`는 생성자에서
  `TokenizeInterface&`/`AssemblerInterface&`/`CheckerInterface&`/`OptimizerInterface&`/
  `ExecuteInterface&`만 받는다. `DebugMode`만 예외적으로 `Executor&`(구체 타입)를 받는데,
  `setStatementHook()`이 `ExecuteInterface`에는 없고 `Executor`에만 있기 때문이다.
- **`optimize()`는 `check()`와 `execute()` 사이에서만 호출**: 세 모드 모두
  `checker_.check(tree)`가 예외 없이 성공한 직후, `executor_.execute(tree)` 이전에
  `optimizer_.optimize(tree)`를 호출한다 — `OptimizerInterface.h`의 계약(의미 오류가
  없는 트리에만 최적화를 적용)을 지키기 위함이다.
- **REPL의 줄 이어받기**: 줄 끝이 `\`면 그 줄을 버퍼에 이어붙이고 다음 줄을 계속
  입력받는다(`... ` 프롬프트). 그 외의 경우(문자열/괄호가 안 닫혀도) 그 줄까지의 내용으로
  즉시 파이프라인을 실행하고 실패하면 오류를 출력한다 — Tokenizer/Assembler 쪽에 "입력이
  아직 끝나지 않음"을 나타내는 별도 예외 타입은 없다.
- **파일 모드의 `{ }` 래핑**: `Assembler::assemble()`은 문장을 하나만 파싱하도록 설계되어
  있어서, 파일 전체를 하나의 `BlockStatement`로 감싸 한 번에 실행한다. `DebugMode`는 이
  래핑 블록 자체는 실행하지 않고 그 안의 top-level 문장들을 하나씩 직접 실행해, 디버거가
  가짜 정지나 depth 밀림 없이 실제 소스의 문장 단위로만 멈추게 한다.
- **`Debugger`는 순수하게 `ExecuteInterface`만 관찰**: `Debugger`는 Mock 없이 실제
  `Executor`의 `environment()`(watch/inspect용)와 `Statement::getLine()`/
  `containsLine()`(브레이크포인트 매칭용)만 사용한다. `next` 명령은 명령을 받은 시점의
  depth를 기억해두고, 이후 그 depth보다 깊은 문장은 건너뛰고 같거나 더 얕은 depth로
  돌아왔을 때만 다시 멈추는 방식(step-over)으로 구현된다.
- **모드별 오류 출력 형식 통일**: `FileRunMode`/`DebugMode`가 파일 경로 문제(디렉터리를
  넘김, 파일을 못 엶)에 쓰는 문구(`Error: path must be a single file: ...` /
  `Error: cannot open file: ...`)를 동일하게 맞춰, 어느 모드로 실행했는지와 무관하게
  같은 종류의 오류가 같은 형태로 보이게 한다.

## 테스트가 다루는 범위

| 파일 | 커버 범위 |
|---|---|
| `CommandLineArgsTest.cpp` | 인자 없음(REPL 기본값), `run`/`debug` + 경로 파싱, 경로 누락/알 수 없는 모드 오류, `--help`/`-h` |
| `RunPromptShellTest.cpp` | 빈 입력/`exit`/빈 줄, 한 줄·여러 줄(백슬래시 이어붙이기) 실행, 세션 동안 `Environment` 상태 공유, 각 Unit이 던지는 오류가 그대로 보고되고 다음 줄 실행에는 영향 없음 |
| `FileRunModeTest.cpp` | 파일 없음/경로가 디렉터리, 파이프라인 각 단계(tokenizer/assembler/checker/executor)가 던진 오류의 보고, 실제 파일을 읽어 실행하는 통합 시나리오(`FileRunModeIntegrationTest`) |
| `DebugModeTest.cpp` | 파일 없음/경로 오류, 파이프라인 앞단 오류 보고, `Debugger` 훅이 실제로 연결되어 브레이크포인트/watch/next/스텝별 정지가 동작하는 통합 시나리오(`DebugModeIntegrationTest`) |
| `DebuggerTest.cpp` | `step`/`next`/`continue`/`break`/`remove`/`breakpoints`/`watch`/`unwatch`/`watches`/`inspect` 명령 각각의 동작, 소스 줄 표시 여부, 입력이 끝났을 때 자동 `continue` 처리 |
