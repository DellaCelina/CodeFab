# IntegrationTest — FileRunMode(파일 모드) 수동/Release 테스트 시나리오

이 폴더는 gtest가 아니라, **Release로 빌드한 `CodeFab.exe`를 직접 실행해서** 공장 제어
쉘의 파일 모드(`CodeFab run <path>`, `Shell/FileRunMode.*`)가 실제로 동작하는지
눈으로 확인하기 위한 샘플 스크립트 모음이다. 각 파일은
`CodeFab/Shell/FileRunModeTest.cpp`(`FileRunModeIntegrationTest`)와
`CodeFab/Shell/RunPromptShellTest.cpp`(`RunPromptShellIntegrationTest`)가 이미
gtest로 검증한 시나리오를 그대로 옮긴 것이라, 여기서 나온 결과가 아래 표와 다르면
회귀가 생긴 것이다.

`DebugIntegrationTest.cpp`는 이 폴더에 함께 있지만 별개다 - 이 파일은 gtest로 빌드되는
실제 단위/통합 테스트(`RunPromptShellIntegrationTest`)이고, 아래 `.txt` 스크립트들은
Release 빌드로 수동 확인하는 샘플이다.

## 실행 방법

Release 빌드 산출물이 있는 `CodeFab\x64\Release` 안에서 실행하는 경우:

```
CodeFab.exe run ..\..\CodeFab\IntegrationTest\01_arithmetic_precedence.txt
```

파일마다 표준출력(stdout)에 찍히는 `print` 결과와 종료 코드(exit code)를 아래 표와
비교해서 확인한다. 정상 실행은 종료 코드 `0`, 실패(문법/의미/런타임 오류)는 `1`이다.

> 참고: `FileRunMode`는 파일 전체 내용을 `"{" + 내용 + "}"`로 감싸 하나의 블록으로
> 실행하므로(`Shell/FileRunMode.cpp` 참고), 한 파일에 여러 top-level 문장을 그대로
> 나열해도 된다 — REPL처럼 한 줄씩 나눠 입력할 필요가 없다.

## 시나리오 목록

| 파일 | 시나리오 | 예상 표준출력 | 예상 결과 |
|---|---|---|---|
| `01_arithmetic_precedence.txt` | 연산자 우선순위(`*`가 `+`보다 먼저) | `7` | 성공(exit 0) |
| `02_parenthesized_expression.txt` | 괄호로 우선순위 재정의 | `9` | 성공(exit 0) |
| `03_string_concatenation.txt` | 문자열 `+` 연결 | `Hello, CodeFab!` | 성공(exit 0) |
| `04_comparison_and_boolean.txt` | 비교 연산자, boolean 리터럴 | `true`\n`false`\n`true`\n`false` | 성공(exit 0) |
| `05_multi_statement_variables.txt` | 여러 top-level 변수 선언 + 재대입 | `30`\n`15` | 성공(exit 0) |
| `06_block_scope_shadowing.txt` | 블록 스코프 shadowing | `inner`\n`global` | 성공(exit 0) |
| `07_if_else.txt` | if-else, dangling-else | `kfc`\n`bbq` | 성공(exit 0) |
| `08_for_loop.txt` | for문 (var 초기화절 포함) | `0`\n`1`\n`2` | 성공(exit 0) |
| `09_error_missing_semicolon.txt` | 세미콜론 누락 → 문법 오류 | (없음) | 실패(exit 1), 오류 메시지: `[line 2] Expect ';' after value. (near '}')` |
| `10_error_unclosed_brace.txt` | 중괄호 미종결 → 문법 오류 | (없음) | 실패(exit 1), 오류 메시지: `Expect '}' after block.` |
| `11_error_duplicate_declaration.txt` | 같은 스코프 변수 중복 선언 → Checker 오류 | (없음) | 실패(exit 1), 오류 메시지: `[line 2] 'a' is already declared in this scope.` |
| `12_error_self_reference.txt` | 초기화식에서 자기 참조 → Checker 오류 | (없음) | 실패(exit 1), 오류 메시지: `[line 1] cannot read local variable in its own initializer.` |
| `13_error_undefined_variable.txt` | 선언되지 않은 변수 참조 → Checker 오류 | (없음) | 실패(exit 1), 오류 메시지: `[line 1] 'notDefined' is not declared.` |
| `14_error_type_mismatch.txt` | 숫자 + 문자열 → Executor 런타임 오류 | (없음) | 실패(exit 1), 오류 메시지: `[line 1] type error: number + string` |
| `15_error_division_by_zero.txt` | 0으로 나누기 → Executor 런타임 오류 | (없음) | 실패(exit 1), 오류 메시지: `[line 1] division by zero.` |
| `16_inheritance_super_call.txt` | 상속 + `Super.method()` 호출(파일 모드 전용 회귀) | `BasicBot`\n`3`\n`FastBot`\n`9`\n`Speeeed!` | 성공(exit 0) |
| `17_inheritance_super_init_and_instanceof.txt` | 상속 + `Super.init()` + `instanceof`(파일 모드 전용 회귀) | `AndOr`\n`10`\n`Zeta`\n`Sam`\n`999`\n`true`\n`true`\n`false`\n`false`\n`false` | 성공(exit 0) |

## 참고

- Tokenizer/Assembler/Checker/Executor가 던지는 오류 메시지는 모두 영어이므로(README.md
  최상위 문서의 "오류 종류와 메시지" 참고) 콘솔 코드페이지와 무관하게 그대로 읽힌다 —
  CLI 인자 파싱 오류(Shell)만 한글이며, 이 폴더의 스크립트들은 그 경로를 타지 않는다.
- 09~15번 시나리오는 파일을 열 수 있는 정상적인 경로를 사용하므로,
  "파일이 존재하지 않음"/"경로가 디렉터리임" 같은 케이스(`FileRunModeTest.cpp`의
  `FileNotFound_...`, `PathIsDirectory_...`)는 실제 파일이 필요 없어 이 폴더에는
  포함하지 않았다 — 존재하지 않는 임의의 경로나 이 폴더 자체를 `run`에 넘겨보면
  재현할 수 있다.
- 16~17번은 다른 시나리오와 달리 `RunPromptShellTest.cpp`가 아니라 파일 모드
  전용으로 추가됐다 — `FileRunMode`는 파일 전체를 `"{" + 내용 + "}"`로 감싸
  실행하므로 클래스 선언이 REPL(각 줄이 전역 스코프)과 달리 블록 스코프 한 단계
  안에 있다. `ClassRuntime::resolveSuperclass`가 depth 캐싱이 아니라 항상 동적
  `lookup`을 쓰는 이유가 바로 이 차이 때문이라(`ClassRuntime.cpp` 주석 참고),
  상속/`Super`/`instanceof`가 파일 모드에서도 실제로 동작하는지 별도로 확인한다.
