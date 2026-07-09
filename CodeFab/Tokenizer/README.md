# Tokenizer

소스코드 문자열을 의미 있는 최소 단위인 `Token`으로 분해하는 Unit입니다. 파이프라인의
가장 앞단(Tokenizer → Assembler → Checker → Executor)에 위치하며, 뒤따르는 Unit은 모두
`Token`/`TokenType`만 알면 되고 원본 문자열을 다시 들여다볼 필요가 없습니다.

## 책임

- 소스 문자열을 한 글자씩 스캔하며 `Token{ type, origin, line }` 목록을 만든다
  (`tokenize(const std::string&)`).
- 리터럴(숫자/문자열/불리언), 키워드, 식별자, 연산자, 구분자를 인식한다.
- 인식할 수 없는 문자를 만나거나 문자열 리터럴이 끝까지 닫히지 않으면 `AssemblyError`를
  던진다(줄 번호 포함).
- 각 토큰에 원본 소스 기준 줄 번호(`line`)를 남겨, 이후 Assembler/Checker/Executor가
  오류 메시지에 `[line N]`을 붙일 수 있게 한다.

## 파일 구성

| 파일 | 역할 |
|---|---|
| `Token.h` | `TokenType` enum, 키워드 문자열 → `TokenType` 매핑(`KEYWORDS`), `Token` 구조체 정의 |
| `TokenizeInterface.h` | `TokenizeInterface` 추상 클래스 + `AssemblyError` 정의 |
| `Tokenizer.h` / `.cpp` | `TokenizeInterface` 구현체. 문자 단위 스캐너 |
| `TokenizerTest.cpp` | Tokenizer 단위 테스트 |

## 핵심 설계

- **한 글자씩 전진하는 스캐너**: `start`/`current`/`line` 세 개의 인덱스만으로 상태를
  관리한다. `scanToken()`이 현재 문자 하나를 보고 `scanString`/`scanNumber`/
  `scanIdentifier`/`scanDefault(연산자·구분자)` 중 하나로 분기한다.
- **키워드는 식별자 스캔의 후처리**: 별도의 키워드 스캐너를 두지 않고, `scanIdentifier()`가
  먼저 알파뉴메릭 식별자를 다 읽은 뒤 `KEYWORDS` 맵에 그 문자열이 있으면 해당
  `TokenType`(예: `Func`, `Class`, `This`, `Super`, `Array`, `import`, `alias`,
  `instanceof`, `and`, `or`)으로, 없으면 `IDENTIFIER`로 토큰을 만든다.
- **두 글자 연산자 우선 매칭**: `=`/`!`/`<`/`>`는 `match()`로 바로 다음 문자가 `=`인지
  먼저 확인해서 `==`/`!=`/`<=`/`>=`와 `=`/`!`/`<`/`>`를 구분한다.
- **줄 번호 추적**: 개행 문자를 지나칠 때(공백 스킵, 문자열 리터럴 내부 포함) `line`을
  증가시켜, 여러 줄에 걸친 토큰도 정확한 줄 번호를 가진다.
- **REPL 멀티라인은 Tokenizer 책임이 아님**: 문자열/괄호가 닫히지 않은 입력을 자동으로
  이어받는 기능은 없다 — Tokenizer는 주어진 문자열을 그 자리에서 끝까지 스캔하며,
  괄호 짝이 맞는지조차 검사하지 않는다(그건 Assembler의 몫). REPL의 여러 줄 입력은
  `Shell/RunPromptShell.cpp`가 줄 끝 `\`만으로 판단한다.

## 테스트가 다루는 범위

| 범주 | 대표 테스트 |
|---|---|
| 리터럴 | `TokenTypes`, `FloatNumber`, `StringLiteral`, `BooleanLiterals` |
| 식별자/키워드 | `Identifier`, `Keywords`, `NewKeywords`(`Func`/`Class`/`This`/`Array`/`import`/`alias`/`instanceof`), `SuperKeyword_IsRecognized`, `AndOrKeywords` |
| 연산자/구분자 | `TwoCharOperators`(`==`/`!=`/`<=`/`>=`), `PercentOperator`, `NewDelimiters`(`.`/`[`/`]`/`,`), `ColonSymbol_IsRecognized`, `MinusAndSlash` |
| 문법 조합 | `DotFieldAccess`, `ArrayIndex`, `FunctionCallWithComma`, `DotDoesNotConflictWithFloat`(`3.14`와 `r.x` 구분) |
| 줄 번호 | `LineTracking` |
| 오류 | `UnterminatedString`, `UnknownCharacter` |
| 괄호 짝 미검사 확인 | `UnclosedParen_DoesNotThrow`, `UnclosedBrace_DoesNotThrow`(Tokenizer는 괄호 짝을 검사하지 않음을 명시) |
| 예약어 보호 | `ArrayIsReservedKeyword`(`Array`를 변수/식별자로 쓸 수 없음) |
