# Assembler

`Tokenizer`가 만든 `Token` 목록을 문법 규칙에 따라 실행 가능한 트리 구조(`SyntaxTree`)로
조립하는 Unit입니다. 재귀 하향(recursive descent) 파서이며, `import` 구문을 처리하기
위해 파일을 재귀적으로 다시 파싱하는 것도 이 Unit의 책임입니다.

## 책임

- `Token` 목록을 받아 `SyntaxTree`(루트가 하나의 `Statement`)를 만든다
  (`assemble(const std::vector<Token>&)`).
- 문법 오류(세미콜론/괄호 누락, 잘못된 대입 대상 등)를 만나면 `AssemblerError`를 던진다
  — 대부분 `[line N] 메시지 (near '토큰')` 형식이다.
- `import "path" alias name;`을 만나면 `SourceReaderInterface`로 해당 파일을 읽고
  토큰화한 뒤, 같은 `Parser`로 그 파일의 최상위 선언들을 재귀적으로 파싱해
  `ImportStatement::declarations`에 담는다. 순환 import는 `importStack_`으로 감지해
  오류를 던진다.
- 모든 `SyntaxTree` 노드(`SyntaxNode` 하위 `Statement`/`Expression`)와 이들을 방문하는
  `SyntaxNodeVisitor` 인터페이스(Visitor 패턴, GoF)의 정의를 소유한다 — `Checker`와
  `Executor`는 이 헤더(`SyntaxTree.h`)만 참조하고 서로를 참조하지 않는다.

## 파일 구성

| 파일 | 역할 |
|---|---|
| `SyntaxTree.h` | `SyntaxNode`/`SyntaxTree`, 모든 `Statement`/`Expression` 노드, `SyntaxNodeVisitor` 정의 |
| `AssemblerInterface.h` | `AssemblerInterface` 추상 클래스 + `AssemblerError` 정의 |
| `Assembler.h` / `.cpp` | `AssemblerInterface` 구현체(재귀 하향 파서, `Parser` 내부 클래스) |
| `SourceReaderInterface.h` | import 대상 파일을 읽어 토큰화까지 마친 결과를 돌려주는 추상 인터페이스 |
| `FileSourceReader.h` / `.cpp` | `SourceReaderInterface`의 실제 파일 시스템 구현체(운영 환경에서 사용, 테스트는 인메모리 Fake로 대체) |
| `AssemblerTest.cpp` | Assembler 단위 테스트 |

## 핵심 설계

- **우선순위 테이블 기반 이항 연산자 파싱**: `kDefaultOperatorPriority`가 낮은
  우선순위부터 높은 순으로 연산자 그룹을 나열한다(`=` → `or` → `and` → `==`/`!=` →
  비교/`instanceof` → `+`/`-` → `*`/`/`/`%`). `parseExpression(level)`은 재귀적으로
  `level+1`을 파싱해 피연산자를 얻고, 자기 레벨의 연산자를 만나는 동안 좌결합으로
  묶는다. `=`(대입)만 예외적으로 우변을 같은 레벨로 재귀시켜 우결합을 만든다.
- **`instanceof`의 특수 처리**: 비교 연산자와 같은 우선순위 레벨에 있지만, 우변이 항상
  클래스 이름(식별자) 하나이므로 재귀적으로 표현식을 파싱하지 않고 토큰 그대로 받는다.
- **후위(postfix) 체인으로 call/필드/인덱스 통합**: `parseCall()`이
  `primary()`를 호출한 뒤 `(...)`(호출), `.name`(필드 접근), `[expr]`(인덱스)이
  연속으로 나오는 동안 계속 감아서, `r.list[0]()`처럼 임의로 조합된 후위 표현식을
  전부 좌결합으로 처리한다.
- **대입 대상 검증은 파싱 시점에**: `makeBinaryExpression`이 `=` 토큰을 만들 때 좌변이
  `IdentifierExpression`/`FieldAccessExpression`/`IndexExpression` 중 하나가 아니면
  즉시 `Invalid assignment target.` 오류를 던진다 — Checker/Executor가 대입 대상
  타입을 다시 검사할 필요가 없다.
- **import는 같은 `Parser`로 재귀 호출**: `SourceReaderInterface::read()`로 얻은 토큰
  목록을, 지금 파싱 중인 파서와 같은 `SyntaxTree`(노드 소유권 공유)를 가리키는 새
  `Parser`에 넘겨 그 파일의 선언들만 파싱한다. `importStack_`(경로 스택)을 공유해
  순환 import를 감지한다.

## 테스트가 다루는 범위

| 범주 | 대표 테스트 |
|---|---|
| 산술/비교/논리 연산자 우선순위·결합 | `SubtractionAssociativityTest`, `DivisionAssociativityTest`, `AndBindsTighterThanOrTest`, `ModuloSamePrecedenceAsMultiplyTest` |
| 문/제어 흐름 | `DeclareStatementTest`, `BlockScopeTest`, `IfElseStatementTest`, `ForStatementTest`, `ForStatementWithVarInitializerTest` |
| 호출/필드/인덱스 후위 체인 | `CallExpressionTest`, `FieldAccessExpressionTest`, `MethodCallExpressionTest`, `IndexExpressionTest`, `ChainedPostfixTest` |
| 함수/클래스 선언 | `FunctionDeclareStatementTest`, `ClassDeclareStatementTest`, `ClassDeclareStatementWithSuperclassTest` |
| 상속(`Super`) | `SuperFieldAccessCallTest`, `SuperFieldAccessWithoutCallTest` |
| `instanceof` | `InstanceOfExpressionTest` |
| 배열 | `ArrayExpressionTest`, `IndexAssignmentTest` |
| 대입 대상 검증 | `InvalidAssignmentTargetThrowsTest`, `InvalidAssignmentTargetNumberThrowsTest`, `ChainedAssignmentTest` |
| 문법 오류 | `MissingSemicolonThrowsTest`, `MissingClosingParenThrowsTest`, `MissingClosingBraceThrowsTest`, `UnexpectedTokenThrowsTest` |
| import | `ImportStatementTest`, `CircularImportThrowsTest`, `FileNotFoundThrowsTest`, `NonDeclarationInsideImportThrowsTest`, `ClassDeclarationInsideImportDoesNotThrowTest`, `ImportThenFieldAccessCallTest` |

## 알려진 제한

- `Checker`/`Executor`와 마찬가지로 `SyntaxNodeVisitor`의 모든 `visit()`이 순수 가상
  함수라, 새 노드 타입을 추가하면 `Optimizer` 등 기존 Visitor 구현체가 컴파일 에러로
  즉시 드러난다(구현을 빠뜨려도 조용히 무시되지 않음).
- `import` 대상 파일 안에서 다시 `import`할 수 있지만, 그 안에서는 `for` 문 안의
  `import`처럼 실행 시점 규칙(반복문 내부 금지 등)은 `Checker`가 검사한다 — `Assembler`
  자체는 문법적으로 파싱 가능한지와 순환 import 여부만 본다.
