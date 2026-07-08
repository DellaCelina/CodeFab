# CodeFab 3일차 확장 구현 가이드

> 이 문서는 `Architecture.md`에서 설계한 3일차 기능(function / class / 정적 배열 /
> 실행전 최적화 / import / 공장 제어 쉘)을 **Tokenizer, Assembler, Checker,
> Executor, Shell을 각각 다른 사람이 맡아 병렬로 구현**할 수 있도록, 모듈별로
> "지금 무엇이 이미 준비되어 있고, 무엇을 채워 넣어야 하는지"를 정리한다.
>
> 모듈 간 계약(교차 모듈 인터페이스: Token, SyntaxTree 노드, Value, 4개
> `*Interface.h`, 생성자 시그니처)은 이미 코드에 반영되어 있고 빌드와 기존
> 133개 테스트가 전부 통과하는 상태다. **이 계약(클래스 이름, 필드 이름, 생성자
> 시그니처)을 바꿔야 할 필요가 생기면 반드시 다른 모듈 담당자와 먼저 상의한다** -
> 혼자 바꾸면 다른 사람의 빌드가 깨진다. 계약 자체를 바꾸지 않고 각 모듈
> 내부에서 채워 넣을 수 있는 부분이 대부분이다.

## 0. 전체 상태 요약

| 모듈 | 이미 준비된 것 (건드릴 필요 없음) | 이번에 구현할 것 |
|---|---|---|
| Tokenizer | 새 `TokenType` 값(§1) | 새 키워드/구분자를 실제로 스캔하는 로직 |
| Assembler | 새 `SyntaxTree` 노드(§2), `Assembler` 생성자(`TokenizeInterface&`, `SourceReaderInterface&`) | 새 문법 파싱, import 재귀 컴파일 |
| Checker | `Checker` 생성자(`ExecuteInterface&`) | 새 노드 의미 검사, Resolver(정적 바인딩), ConstantFolder(상수 폴딩) |
| Executor | `Value`/`Environment`/`ExecuteInterface` 확장(§4), `IdentifierExpression`/`AssignExpression` 처리는 이미 `depth`/`target`을 쓰도록 배선됨 | 새 노드 실행 로직(함수 호출, 클래스, 배열, import, instanceof) |
| Shell | - | `FileRunMode`, `DebugMode`, `Debugger`, `CommandLineArgs`, `main.cpp` CLI 분기 |

각 절은 "현재 상태(코드에 이미 있음) → 할 일 → 예시 코드/의사코드 → 테스트
가이드" 순서로 구성했다.

## 1. Tokenizer 담당자 가이드

### 현재 상태

`Tokenizer/Token.h`의 `TokenType`에 아래가 이미 추가되어 있다(주석 참고).

```cpp
FUNC, RETURN, CLASS, THIS, ARRAY, IMPORT, ALIAS, INSTANCEOF,   // 키워드
DOT, LEFT_BRACKET, RIGHT_BRACKET, COMMA,                       // 구분자
```

`Tokenizer.cpp`/`Tokenizer.h`는 **아직 이 토큰들을 만들지 않는다.** `Token`
구조체(`type`, `origin`, `line`)와 `TokenizeInterface`(`tokenize(source)`)는
바뀌지 않는다.

### 할 일

1. **키워드 인식**: `Tokenizer.cpp`의 `KEYWORDS` 맵에 아래를 추가한다.

```cpp
static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    { "var",   TokenType::VAR   },
    { "print", TokenType::PRINT },
    { "if",    TokenType::IF    },
    { "else",  TokenType::ELSE  },
    { "for",   TokenType::FOR   },
    { "true",  TokenType::TRUE  },
    { "false", TokenType::FALSE },
    // 3일차 확장
    { "Func",       TokenType::FUNC       },
    { "return",     TokenType::RETURN     },
    { "Class",      TokenType::CLASS      },
    { "This",       TokenType::THIS       },
    { "Array",      TokenType::ARRAY      },
    { "import",     TokenType::IMPORT     },
    { "alias",      TokenType::ALIAS      },
    { "instanceof", TokenType::INSTANCEOF },
};
```

   대소문자는 슬라이드 예시(`Func`, `Class`, `This`, `Array`는 대문자 시작,
   `import`/`alias`/`instanceof`/`return`은 소문자)를 그대로 따랐다. 팀
   컨벤션으로 바꾸고 싶으면 Assembler 담당자와 맞춰서 결정한다(Assembler는
   `TokenType`만 보고 파싱하므로 원문 대소문자가 달라져도 영향 없음).

2. **새 구분자 스캔**: `Tokenizer.cpp`의 `scanToken()` switch에 한 줄씩만
   추가하면 된다(기존 한 글자 토큰들과 동일한 패턴).

```cpp
case '.': addToken(TokenType::DOT);           break;
case '[': addToken(TokenType::LEFT_BRACKET);  break;
case ']': addToken(TokenType::RIGHT_BRACKET); break;
case ',': addToken(TokenType::COMMA);         break;
```

   `.`이 숫자 리터럴(`3.14`)의 소수점과 충돌하지 않는지 확인한다 - 현재
   `scanNumber()`는 `peek() == '.' && isDigit(peekNext())`일 때만 소수점을
   숫자의 일부로 소비하므로, `foo.bar`처럼 식별자 뒤에 오는 `.`은
   `scanIdentifier()`가 알파벳으로 끝나는 지점에서 멈추고 다음 `scanToken()`
   호출에서 별도의 `DOT` 토큰으로 처리된다 - 이미 안전하다.

3. **`Array`가 예약어가 되면서 생기는 부작용 확인**: 기존에 `Array`라는
   이름의 변수를 쓰던 테스트/스크립트가 있다면 `IDENTIFIER`가 아니라 `ARRAY`로
   토큰화된다. `TokenizerTest.cpp`에 새 키워드/구분자 각각에 대한 테스트를
   추가할 때 이 점도 함께 확인한다.

### 테스트 가이드

`Tokenizer/TokenizerTest.cpp`에 새 키워드/구분자마다 최소 1개씩 케이스를
추가한다(기존 테스트 스타일 그대로: 입력 문자열을 `tokenize()`하고
`TokenType`/`origin`/`line`을 검증). 예:

```cpp
TEST(TokenizerTest, FuncKeyword_IsRecognized) {
    Tokenizer tokenizer;
    auto tokens = tokenizer.tokenize("Func");
    ASSERT_EQ(tokens[0].type, TokenType::FUNC);
}
```

## 2. Assembler 담당자 가이드

### 현재 상태

`Assembler/SyntaxTree.h`에 아래 노드가 이미 정의되어 있다(전부
`operator==` 구현 포함, 다른 모듈이 이미 이 필드명을 안다고 가정하고
작업 중이니 이름을 바꾸지 않는다).

| 노드 | 필드 |
|---|---|
| `CallExpression` | `callee: Expression*`, `arguments: vector<Expression*>` |
| `FieldAccessExpression` | `object: Expression*`, `name: Token` |
| `ThisExpression` | (없음, 토큰만) |
| `ArrayExpression` | `sizeExpr: Expression*` |
| `IndexExpression` | `collection: Expression*`, `index: Expression*` |
| `InstanceOfExpression` | `object: Expression*`, `className: Token` |
| `FunctionDeclareStatement` | `name: Token`, `params: vector<Token>`, `body: vector<Statement*>` |
| `ReturnStatement` | `value: Expression*` (nullable) |
| `ClassDeclareStatement` | `name: Token`, `methods: vector<FunctionDeclareStatement*>` |
| `ImportStatement` | `alias: Token`, `declarations: vector<Statement*>` |

`AssignExpression`은 `identifier` 대신 **`target: Expression*`**로 일반화되어
있다 - `a = 3`이면 `target`이 `IdentifierExpression*`, 나중에 필드/배열 대입을
추가하면 `FieldAccessExpression*`/`IndexExpression*`이 온다.

`IdentifierExpression`에 `mutable std::optional<int> depth;` 필드가 있다 -
**Assembler는 이 필드를 절대 건드리지 않는다.** Checker가 채워 넣는다.

`Assembler` 클래스는 이제 생성자를 받는다.

```cpp
Assembler(TokenizeInterface& tokenizer, SourceReaderInterface& sourceReader);
```

`SourceReaderInterface`(`Assembler/SourceReaderInterface.h`)는 `read(path)`
하나만 있는 파일 읽기 추상화이고, 운영용 구현체 `FileSourceReader`가 이미
있다(`Assembler/FileSourceReader.h/.cpp`, 실제 `ifstream`으로 읽음). 테스트에서는
이 두 협력자를 직접 만들어 넣으면 된다(`AssemblerTest.cpp`가 이미 이렇게
되어 있다 - 새 테스트도 같은 패턴을 따르면 됨).

### 문법 규칙 (추가분)

기존 문법에 아래를 추가한다. `expression(operatorPriority.size())`가 지금은
`unary`로 바로 떨어지는데, `unary`와 `primary` 사이에 **postfix 체인**을 하나
끼워 넣는 것이 핵심이다.

```
statement    -> ... | funcDeclStmt | classDeclStmt | returnStmt | importStmt
funcDeclStmt -> FUNC IDENTIFIER LEFT_PAREN params? RIGHT_PAREN blockStmt
params       -> IDENTIFIER (COMMA IDENTIFIER)*
classDeclStmt-> CLASS IDENTIFIER LEFT_BRACE funcDeclStmt* RIGHT_BRACE
returnStmt   -> RETURN expression(0)? SEMICOLON
importStmt   -> IMPORT STRING ALIAS IDENTIFIER SEMICOLON

unary        -> (MINUS | BANG) unary | call
call         -> primary ( "(" arguments? ")" | "." IDENTIFIER | "[" expression(0) "]" )*
arguments    -> expression(0) (COMMA expression(0))*
primary      -> NUMBER | STRING | TRUE | FALSE | THIS | IDENTIFIER
              | ARRAY "(" expression(0) ")"
              | LEFT_PAREN expression(0) RIGHT_PAREN
```

`instanceof`는 비교 연산자와 비슷한 층위에 새 우선순위 레벨로 추가하거나
(`kDefaultOperatorPriority`에 `{ TokenType::INSTANCEOF }` 레벨을 하나
끼워 넣는다), 별도로 `parseExpression`이 끝난 뒤 후처리로 파싱해도 된다 - 우변이
항상 식별자(클래스 이름)여야 하므로 이항 연산자와 다르게 `Expression*`이 아니라
`Token`으로 받아야 한다는 점만 유의한다(`makeBinaryExpression`을 그대로
재사용할 수 없다).

### 구현 순서 제안

1. **postfix 체인부터**: `parseUnary()`가 지금 바로 `parsePrimary()`를
   호출하는데, 그 사이에 `parseCall()`을 끼워 넣는다.

```cpp
Expression* parseUnary() {
    if (auto token = currentToken(); token && isUnaryOperator(token->type)) {
        Token opToken = popToken();
        Expression* operand = parseUnary();
        return makeUnaryExpression(opToken, operand);
    }
    return parseCall();
}

Expression* parseCall() {
    Expression* expr = parsePrimary();
    while (auto token = currentToken()) {
        if (token->type == TokenType::LEFT_PAREN) {
            Token leftParen = popToken();
            std::vector<Expression*> args;
            if (auto t = currentToken(); t && t->type != TokenType::RIGHT_PAREN) {
                args.push_back(parseExpression(0));
                while (auto comma = currentToken(); comma && comma->type == TokenType::COMMA) {
                    popToken();
                    args.push_back(parseExpression(0));
                }
            }
            Token rightParen = popExpectedToken(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");
            expr = addNode<CallExpression>(Tokens{ leftParen, rightParen }, expr, args);
        } else if (token->type == TokenType::DOT) {
            Token dot = popToken();
            Token name = popExpectedToken(TokenType::IDENTIFIER, "Expect property name after '.'.");
            expr = addNode<FieldAccessExpression>(Tokens{ dot }, expr, name);
        } else if (token->type == TokenType::LEFT_BRACKET) {
            Token leftBracket = popToken();
            Expression* index = parseExpression(0);
            Token rightBracket = popExpectedToken(TokenType::RIGHT_BRACKET, "Expect ']' after index.");
            expr = addNode<IndexExpression>(Tokens{ leftBracket, rightBracket }, expr, index);
        } else {
            break;
        }
    }
    return expr;
}
```

2. **`primary()`에 `THIS`/`ARRAY` 분기 추가**:

```cpp
case TokenType::THIS:
    popToken();
    return addNode<ThisExpression>(Tokens{ *token });
case TokenType::ARRAY: {
    popToken();
    popExpectedToken(TokenType::LEFT_PAREN, "Expect '(' after 'Array'.");
    Expression* sizeExpr = parseExpression(0);
    popExpectedToken(TokenType::RIGHT_PAREN, "Expect ')' after array size.");
    return addNode<ArrayExpression>(Tokens{ *token }, sizeExpr);
}
```

3. **대입 좌변 검사 확장** (`makeBinaryExpression`의 `EQUAL` 케이스): 지금은
   `dynamic_cast<IdentifierExpression*>(left)`만 허용한다. 필드/배열 대입을
   지원하려면 아래처럼 넓힌다(주의: `AssignExpression`의 생성자 파라미터
   타입이 이미 `Expression*`이라 캐스트 결과를 그대로 넘기면 된다).

```cpp
case TokenType::EQUAL: {
    if (!dynamic_cast<IdentifierExpression*>(left)
        && !dynamic_cast<FieldAccessExpression*>(left)
        && !dynamic_cast<IndexExpression*>(left)) {
        throw makeParseError("Invalid assignment target.", opToken);
    }
    return addNode<AssignExpression>(Tokens{ opToken }, left, right);
}
```

4. **함수/클래스 선언**: `parseStatement()`의 switch에 `FUNC`/`CLASS`/`RETURN`/
   `IMPORT` 분기를 추가한다. 클래스 바디의 메서드는 `FUNC` 키워드로 시작하는
   `parseFunction()`을 그대로 반복 호출하면 된다(별도 파싱 로직 불필요 -
   Architecture.md §4.1 "메서드도 Func로 통일" 참고).

```cpp
FunctionDeclareStatement* parseFunction() {
    Token funcToken = popToken(); // FUNC
    Token name = popExpectedToken(TokenType::IDENTIFIER, "Expect function name.");
    popExpectedToken(TokenType::LEFT_PAREN, "Expect '(' after function name.");
    std::vector<Token> params;
    if (auto t = currentToken(); t && t->type != TokenType::RIGHT_PAREN) {
        params.push_back(popExpectedToken(TokenType::IDENTIFIER, "Expect parameter name."));
        while (auto comma = currentToken(); comma && comma->type == TokenType::COMMA) {
            popToken();
            params.push_back(popExpectedToken(TokenType::IDENTIFIER, "Expect parameter name."));
        }
    }
    Token rightParen = popExpectedToken(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
    Token leftBrace = popExpectedToken(TokenType::LEFT_BRACE, "Expect '{' before function body.");
    std::vector<Statement*> body;
    while (auto t = currentToken(); t && t->type != TokenType::RIGHT_BRACE) {
        body.push_back(static_cast<Statement*>(parseStatement()));
    }
    Token rightBrace = popExpectedToken(TokenType::RIGHT_BRACE, "Expect '}' after function body.");
    return addNode<FunctionDeclareStatement>(
        Tokens{ funcToken, rightParen, leftBrace, rightBrace }, name, params, body);
}

ClassDeclareStatement* parseClass() {
    Token classToken = popToken();
    Token name = popExpectedToken(TokenType::IDENTIFIER, "Expect class name.");
    Token leftBrace = popExpectedToken(TokenType::LEFT_BRACE, "Expect '{' before class body.");
    std::vector<FunctionDeclareStatement*> methods;
    while (auto t = currentToken(); t && t->type != TokenType::RIGHT_BRACE) {
        methods.push_back(parseFunction());
    }
    Token rightBrace = popExpectedToken(TokenType::RIGHT_BRACE, "Expect '}' after class body.");
    return addNode<ClassDeclareStatement>(Tokens{ classToken, leftBrace, rightBrace }, name, methods);
}
```

   `return`은 함수 안에서만 허용되는지 여부는 **문법 검사가 아니라 의미
   검사**이므로 Assembler는 신경 쓰지 않는다(Checker가 처리, §3 참고) - 파서는
   그냥 어디서든 `return;`/`return expr;`을 파싱할 수 있게 해도 된다.

5. **import 재귀 컴파일**: 이게 Assembler에서 가장 까다로운 부분이다
   (Architecture.md §7.2). 순서:

```cpp
ImportStatement* parseImport() {
    Token importToken = popToken();
    Token pathToken = popExpectedToken(TokenType::STRING, "Expect a file path string after 'import'.");
    popExpectedToken(TokenType::ALIAS, "Expect 'alias' after import path.");
    Token aliasToken = popExpectedToken(TokenType::IDENTIFIER, "Expect alias name.");
    Token semicolon = popExpectedToken(TokenType::SEMICOLON, "Expect ';' after import statement.");

    // 순환 감지: 지금 import 하려는 파일이 이미 "해석 중" 스택에 있으면 오류.
    if (std::find(importStack_.begin(), importStack_.end(), pathToken.origin) != importStack_.end()) {
        throw AssemblerError("순환 import: '{}'", pathToken.origin);
    }
    importStack_.push_back(pathToken.origin);

    std::string content;
    try {
        content = sourceReader_.read(pathToken.origin);
    } catch (const std::exception& e) {
        importStack_.pop_back();
        throw AssemblerError("import 대상 파일을 열 수 없습니다: '{}' ({})", pathToken.origin, e.what());
    }

    std::vector<Token> importedTokens = tokenizer_.tokenize(content);
    SyntaxTree importedTree = assemble(importedTokens);  // 재귀 호출!
    importStack_.pop_back();

    // 최상위가 단일 Statement(보통 BlockStatement)라고 가정하고 그 안의
    // 선언들만 추려서 ImportStatement에 담는다. VarDeclareStatement/
    // FunctionDeclareStatement 외의 문장이 섞여 있으면 팀 컨벤션에 따라
    // 무시하거나 AssemblerError를 던진다("선언 외 내용은 허용하지 않음").
    std::vector<Statement*> declarations = extractDeclarations(importedTree.getRoot());

    return addNode<ImportStatement>(Tokens{ importToken, semicolon }, aliasToken, declarations);
}
```

   주의할 점:
   - `importStack_`(예: `std::vector<std::string>`)은 `Assembler`(또는
     `Parser`, 재귀 호출 구조에 따라)의 인스턴스 멤버로 둔다. `assemble()`이
     매번 새 `Parser`를 만드므로, `Assembler` 클래스 레벨에 두고
     `Parser` 생성 시 참조로 넘기는 방식을 권장한다.
   - `importedTree`가 소유한 노드들(`unique_ptr`)이 함수가 끝나면서 사라지면
     안 되므로, `importedTree`의 노드 소유권을 **현재 만들고 있는 트리로
     옮겨야 한다.** `SyntaxTree`에 이런 이동을 돕는 메서드가 없다면
     추가한다(예: `void adopt(SyntaxTree&& other);` — 이 메서드는 새로
     추가해야 하는 인터페이스이니, 추가하게 되면 Checker/Executor 담당자에게
     공유한다. 다만 두 모듈 다 `SyntaxTree`를 값으로만 다루지 내부 구현을
     직접 건드리지 않으므로 영향은 없을 것이다).
   - "선언에 대한 내용만 허용"의 엄격도(오류 처리 vs 무시)는 팀 컨벤션으로
     정한다.
   - 캐싱(같은 파일을 여러 번 import할 때 재컴파일 생략)은 **이번 범위에서
     하지 않는다** - 매번 다시 읽고 다시 파싱해도 정확성에는 문제없다.

### 테스트 가이드

`AssemblerTest.cpp`는 이미 `Tokenizer`+`FileSourceReader`를 실제로 주입하는
픽스처로 바뀌어 있다. import를 테스트하려면 `SourceReaderInterface`의 **Fake**
(맵 기반 인메모리 구현체)를 별도로 만들어서 그 테스트에서만 주입하는 것을
권장한다(실제 파일을 만들지 않고도 순환 감지/파일 없음 케이스를 테스트할 수
있음). 새 문법(함수/클래스/배열/instanceof)은 기존 테스트들과 동일한 패턴
(토큰 목록을 손으로 만들고 `assembler.assemble(tokens)` 결과를 손으로 만든
golden 트리와 `operator==`로 비교)으로 작성한다.

## 3. Checker 담당자 가이드

### 현재 상태

`Checker` 생성자가 `ExecuteInterface&`를 받는다.

```cpp
explicit Checker(ExecuteInterface& executor);
```

`executor_`는 이미 멤버로 저장되어 있다(`Checker.h`). `check(SyntaxTree&)`
시그니처는 바뀌지 않았다. `checkStatement`/`checkExpression`의 dynamic_cast
기반 분기 구조도 그대로다 - 새 노드 타입마다 `else if` 분기를 추가하면 된다.

### 할 일 1: 새 노드 의미 검사

기존 패턴(`checkBlock`, `checkDeclare`, ...)을 그대로 따라 아래를 추가한다.

- **함수** (Architecture.md §3.2): "현재 함수 안인지" 카운터를 하나 두고
  (`int functionDepth = 0;`), `FunctionDeclareStatement`의 `body`를 검사하는
  동안 증가/감소시킨다. `ReturnStatement`를 만났는데 `functionDepth == 0`이면
  `reportError(...)`. 파라미터 이름 중복은 `params`를 순회하며
  `unordered_set`으로 검사.
- **클래스** (Architecture.md §4.2): "현재 클래스/메서드 안인지" 상태를 하나
  더 두고, `ClassDeclareStatement`의 각 메서드를 검사하는 동안 켠다.
  `ThisExpression`을 만났는데 클래스 밖이면 오류. 메서드 이름이 `"init"`이고
  `ReturnStatement`에 `value != nullptr`이면 오류("init은 return 없음").
- **import** (Architecture.md §7.3): 스코프 스택에 "이 스코프에서 import한
  path 집합"을 추가로 들고 다니거나(`scopes`를 확장하거나 병행 스택을 두거나),
  `ImportStatement`를 만날 때마다 (1) 같은 스코프 내 동일 path 재import, (2)
  상위 스코프에서 이미 import한 path의 재import, (3) 반복문(`ForStatement`)
  바디 내부에서의 import를 검사한다. **파일 존재/순환 검사는 이미 Assembler가
  끝냈으므로 Checker는 하지 않는다** - `ImportStatement`가 노드로 존재한다는
  것 자체가 "그 파일은 문제없이 컴파일됐다"는 뜻이다.
- **배열/인스턴스 관련 검사**: 대부분 런타임에만 알 수 있는 값의 실제
  타입에 달려 있으므로(Architecture.md §4.2, §5.2) Checker에서는 대부분
  검사하지 않는다. 정적으로 확정 가능한 것만(예: 자기 자신을 상속하는 것처럼
  이번 범위엔 없는 경우) 추가한다.

### 할 일 2: Resolver (정적 바인딩)

`Architecture.md §6.1` 설계를 그대로 구현한다. 핵심은 "지금 몇 단계 스코프를
올라가야 이 이름이 선언된 스코프가 나오는가"를 계산해서
`IdentifierExpression::depth`(이미 필드로 존재, `mutable`이라 const 트리에도
써도 됨)에 저장하는 것이다.

```cpp
void Checker::resolveIdentifier(IdentifierExpression* id) {
    for (int distance = 0; distance < static_cast<int>(scopes.size()); ++distance) {
        auto& scope = scopes[scopes.size() - 1 - distance]; // 안쪽부터
        if (scope.count(id->name)) {
            id->depth = distance;
            return;
        }
    }
    id->depth = std::nullopt; // 전역이거나 못 찾음 - Executor가 동적 조회로 폴백
}
```

이 계산은 기존 `checkIdentifier`(선언 여부 검사)와 같은 시점에 같이 해도 되고
(검사 통과 후 바로 `resolveIdentifier` 호출), 검사가 전부 끝난 뒤 트리를 한 번
더 순회하는 별도 패스로 짜도 된다. **주의**: `AssignExpression::target`이
`IdentifierExpression`인 경우도 같은 노드 타입이므로 `checkExpression`이
`AssignExpression`을 처리하는 분기(현재는 없음 - 추가해야 함)에서
`target`을 `IdentifierExpression*`으로 캐스트해 같은 `resolveIdentifier`를
호출하면 읽기/쓰기 양쪽에 자동 적용된다.

`ThisExpression`도 "메서드 호출 시 스코프에 this라는 이름으로 바인딩된
변수"로 취급하면(§4.3), 클래스 검사 로직이 메서드 바디에 들어갈 때 가상의
`this` 이름을 스코프에 `declare("this")`해두는 것만으로 나머지는 그대로
동작한다.

### 할 일 3: ConstantFolder (상수 연산 최적화)

`Architecture.md §6.2` 설계대로, **산술을 다시 구현하지 말고
`executor_.evaluate()`를 호출**한다.

```cpp
Expression* Checker::foldConstants(Expression* expr) {
    if (auto* bin = dynamic_cast<BinaryExpression*>(expr)) {
        // 먼저 자식들을 접는다 (bottom-up). BinaryExpression::left/right는
        // 이제 const가 아니므로 아래처럼 덮어쓸 수 있다.
        bin->left = foldConstants(bin->left);
        bin->right = foldConstants(bin->right);

        bool leftIsLiteral = dynamic_cast<NumberExpression*>(bin->left)
            || dynamic_cast<StringExpression*>(bin->left)
            || dynamic_cast<BooleanExpression*>(bin->left);
        bool rightIsLiteral = dynamic_cast<NumberExpression*>(bin->right)
            || dynamic_cast<StringExpression*>(bin->right)
            || dynamic_cast<BooleanExpression*>(bin->right);

        if (leftIsLiteral && rightIsLiteral) {
            try {
                Value result = executor_.evaluate(bin); // 실제 실행 로직 재사용!
                return replaceWithLiteral(result, bin);  // Value -> 리터럴 노드로 변환
            } catch (const ExecutorError&) {
                // 0으로 나누기 등 - 접지 않고 원래 트리를 그대로 둔다. 런타임에
                // Executor가 같은 지점에서 다시 오류를 내야 정확한 문맥이 된다.
            }
        }
    }
    return expr;
}
```

`replaceWithLiteral`은 `Value::type()`에 따라 `NumberExpression`/
`StringExpression`/`BooleanExpression` 중 하나를 새로 만들어(`SyntaxTree`에
`add()`로 등록) 반환하는 헬퍼다. **주의**: `foldConstants`가 새로 만든 노드를
`tree.add()`로 등록하려면 이 함수가 `SyntaxTree&`에 접근할 수 있어야
한다 - `check(SyntaxTree& tree)` 안에서 호출되므로 파라미터로 넘기면 된다.

이 패스는 `SemanticAnalyzer`(기존 의미 검사)가 **통과한 뒤에만** 실행한다
(`check()`가 예외 없이 끝까지 순회를 마쳤을 때). `BinaryExpression`의 자식
포인터를 덮어쓰는 것이므로, `ForStatement`의 `init`/`compare`/`next`처럼
`Statement*`/`Expression*` 필드를 가진 다른 노드들도 그 필드가 `BinaryExpression`
등을 가리키고 있다면 마찬가지로 `foldConstants()`의 반환값으로 덮어써야
폴딩 결과가 실제로 반영된다(단순히 재귀 호출만 하고 반환값을 버리면 안 됨).
루프 안(예: `ForStatement::loop`)에 있는 문장들까지 재귀적으로 순회하도록
`checkStatement`와 나란히 `foldStatement(Statement*)` 헬퍼를 만드는 것을
권장한다.

### 테스트 가이드

- 기존 `CheckerTest.cpp` 패턴(SyntaxNode를 손으로 만들고 `checker.check(tree)`
  호출)을 그대로 따른다. 이제 `Checker checker(executor);`처럼 실제
  `Executor` 인스턴스(또는 Fake `ExecuteInterface`)를 만들어 넘겨야 한다.
- Resolver 테스트: `IdentifierExpression`을 만들고 `check()` 호출 후
  `id->depth`가 기대한 값인지 직접 확인한다.
- ConstantFolder 테스트: `check()` 호출 후 트리를 순회해서 원래
  `BinaryExpression`이던 자리에 리터럴 노드가 들어갔는지 확인한다. Test
  Double(Architecture.md §6.3)로 `evaluate()` 호출 횟수를 세는 방식도
  고려한다(Fake `ExecuteInterface`를 만들어 카운터를 증가시키는 `evaluate()`
  구현).

## 4. Executor 담당자 가이드

### 현재 상태

- `Value`(`Executor/Value.h`)에 `Function`/`Class`/`Instance`/`Array`/`Module`
  타입이 이미 있다. `Value(const FunctionDeclareStatement*)`,
  `Value(const ClassDeclareStatement*)`, `Value(std::shared_ptr<InstanceValue>)`,
  `Value(std::shared_ptr<ArrayValue>)`, `Value(std::shared_ptr<Scope>)` 생성자와
  `isFunction()`/`asFunction()` 등 대응 accessor가 전부 구현되어 있다.
- `Executor/InstanceValue.h`: `{ const ClassDeclareStatement* klass;
  std::shared_ptr<Scope> fields; }`. 필드 저장소로 기존 `Scope`를 그대로
  쓴다 - `fields->define(name, value)`(없으면 생성/있으면 갱신),
  `fields->get(name)`(없으면 nullopt), `fields->assign(name, value)`(있을 때만
  갱신, false 반환 가능)를 그대로 쓰면 된다.
- `Executor/ArrayValue.h`: `{ std::vector<Value> items; }`.
- `Environment`에 `lookupAt(distance, name)`/`assignAt(distance, name, value)`가
  이미 구현되어 있다. `Executor.cpp`의 `IdentifierExpression`/`AssignExpression`
  핸들러는 **이미** `identifier->depth`가 있으면 `*At` 계열을, 없으면 기존
  동적 조회를 쓰도록 분기해뒀다 - Checker의 Resolver가 완성되면 자동으로
  빨라진다. 이 부분은 손댈 필요 없다.
- `AssignExpression`의 대상은 이제 `target`(과거 `identifier`)이고, 현재
  핸들러는 `dynamic_cast<IdentifierExpression*>(assign->target)`이 실패하면
  `ExecutorError("아직 지원하지 않는 대입 대상입니다.")`를 던진다 - 필드/배열
  대입을 구현하면 이 자리에 분기를 추가한다.
- `ExecuteInterface`에 `evaluate(Expression*)`(기존 `Executor::evaluate`가
  이제 `override`), `environment() const`(디버그 모드용, 이미 구현됨)가
  추가되어 있다.
- `Executor`에 `setStatementHook(std::function<void(Statement*)>)`이 이미
  있고, `execute(Statement*)`가 매번 이 훅을 호출하도록 배선되어 있다(훅이
  없으면(`nullptr`) 기존과 동일하게 동작 - 이미 확인됨, 기존 133개 테스트
  통과).

### 할 일 1: 함수 선언/호출

Architecture.md §3.3을 그대로 구현한다.

```cpp
statementHandlers_[std::type_index(typeid(FunctionDeclareStatement))] = [this](Statement* stmt) {
    auto* decl = static_cast<FunctionDeclareStatement*>(stmt);
    environment_.define(decl->name.origin, Value(decl)); // 선언 시점에 이름부터 등록 -> 재귀 자연 지원
};
```

`CallExpression` 핸들러(신규)는 `expressionHandlers_`에 등록한다. callee가
`IdentifierExpression`인 일반 함수 호출과, callee가
`FieldAccessExpression`인 메서드 호출을 나눠서 처리한다(§4.3 참고, 클래스
항목에서 이어서 설명).

```cpp
expressionHandlers_[std::type_index(typeid(CallExpression))] = [this](Expression* expr) {
    auto* call = static_cast<CallExpression*>(expr);

    if (auto* fieldAccess = dynamic_cast<FieldAccessExpression*>(call->callee)) {
        return callMethod(fieldAccess, call->arguments); // 아래 "클래스" 절 참고
    }

    Value callee = evaluate(call->callee);
    std::vector<Value> args;
    for (Expression* arg : call->arguments) {
        args.push_back(evaluate(arg));
    }

    if (callee.isFunction()) {
        return callFunction(callee.asFunction(), args, /*boundThis=*/std::nullopt);
    }
    if (callee.isClass()) {
        return instantiate(callee.asClass(), args); // 아래 "클래스" 절 참고
    }
    throw ExecutorError("호출할 수 없는 대상입니다.");
};
```

`callFunction` 헬퍼(새로 추가):

```cpp
Value Executor::callFunction(const FunctionDeclareStatement* decl, const std::vector<Value>& args,
                              std::optional<Value> boundThis) {
    if (args.size() != decl->params.size()) {
        throw ExecutorError("'{}' 함수는 인자 {}개가 필요합니다 (전달된 인자: {}개)",
            decl->name.origin, decl->params.size(), args.size());
    }
    environment_.pushScope();
    try {
        if (boundThis) {
            environment_.define("this", *boundThis);
        }
        for (size_t i = 0; i < decl->params.size(); ++i) {
            environment_.define(decl->params[i].origin, args[i]);
        }
        for (Statement* stmt : decl->body) {
            execute(stmt);
        }
    } catch (const ReturnSignal& ret) {
        environment_.popScope();
        return ret.value;
    } catch (...) {
        environment_.popScope();
        throw;
    }
    environment_.popScope();
    return Value(); // return 없이 끝나면 Nil
}
```

`ReturnSignal`은 **Executor 내부 전용**(공개 헤더에 노출하지 않음, `Executor.cpp`의
익명 네임스페이스에 정의)이다.

```cpp
namespace {
struct ReturnSignal {
    Value value;
};
}  // namespace
```

`ReturnStatement` 핸들러:

```cpp
statementHandlers_[std::type_index(typeid(ReturnStatement))] = [this](Statement* stmt) {
    auto* ret = static_cast<ReturnStatement*>(stmt);
    throw ReturnSignal{ ret->value ? evaluate(ret->value) : Value() };
};
```

**주의**: `ScopeGuard`(기존, `BlockStatement`/`ForStatement`에서 씀)는 소멸자에서
`popScope()`를 부르므로 `ReturnSignal`처럼 예외로 스코프를 빠져나가도 안전하게
동작한다. 함수 호출용 스코프도 `ScopeGuard`를 재사용할 수 있는지 검토하라 -
위 예시처럼 수동 try/catch로 짜도 되고, `ScopeGuard guard(environment_);`로
바꾸면 `catch (...) { environment_.popScope(); throw; }` 블록이 필요 없어져
더 간결해진다.

### 할 일 2: 클래스

Architecture.md §4.3을 그대로 구현한다.

```cpp
statementHandlers_[std::type_index(typeid(ClassDeclareStatement))] = [this](Statement* stmt) {
    auto* decl = static_cast<ClassDeclareStatement*>(stmt);
    environment_.define(decl->name.origin, Value(decl));
};

Value Executor::instantiate(const ClassDeclareStatement* klass, const std::vector<Value>& args) {
    auto instance = std::make_shared<InstanceValue>();
    instance->klass = klass;
    instance->fields = std::make_shared<Scope>();
    Value instanceValue(instance);

    for (FunctionDeclareStatement* method : klass->methods) {
        if (method->name.origin == "init") {
            callFunction(method, args, instanceValue); // 반환값은 버림
            break;
        }
    }
    return instanceValue;
}

Value Executor::callMethod(FieldAccessExpression* fieldAccess, const std::vector<Expression*>& argExprs) {
    Value object = evaluate(fieldAccess->object);
    if (!object.isInstance()) {
        throw ExecutorError("인스턴스가 아닌 대상의 메서드를 호출했습니다.");
    }
    auto& instance = object.asInstance();
    FunctionDeclareStatement* method = nullptr;
    for (FunctionDeclareStatement* m : instance->klass->methods) {
        if (m->name.origin == fieldAccess->name.origin) {
            method = m;
            break;
        }
    }
    if (!method) {
        throw ExecutorError("'{}' 메서드가 존재하지 않습니다.", fieldAccess->name.origin);
    }
    std::vector<Value> args;
    for (Expression* arg : argExprs) {
        args.push_back(evaluate(arg));
    }
    return callFunction(method, args, object);
}
```

`FieldAccessExpression` 핸들러(값 읽기 문맥, 호출이 아닐 때):

```cpp
expressionHandlers_[std::type_index(typeid(FieldAccessExpression))] = [this](Expression* expr) {
    auto* access = static_cast<FieldAccessExpression*>(expr);
    Value object = evaluate(access->object);
    if (!object.isInstance()) {
        throw ExecutorError("인스턴스가 아닌 대상의 필드에 접근했습니다.");
    }
    auto& instance = object.asInstance();
    if (auto field = instance->fields->get(access->name.origin)) {
        return *field;
    }
    // 필드에 없으면 메서드를 "바인딩된 함수 값"처럼 반환하고 싶을 수 있지만,
    // 현재 설계는 메서드 호출을 CallExpression 쪽(callMethod)에서 전담하므로
    // 값 읽기 문맥에서 메서드에 접근하면 에러로 처리해도 충분하다. 필드에
    // 저장된 함수를 호출하는 시나리오까지 지원하려면 Architecture.md §4.3의
    // "확장 지점" 설명을 참고해 여기서 메서드도 조회하도록 넓힌다.
    throw ExecutorError("'{}' 필드가 존재하지 않습니다.", access->name.origin);
};
```

`ThisExpression` 핸들러: `IdentifierExpression`과 동일하게 취급한다(§4.3).
가장 간단한 방법은 `ThisExpression`도 `depth`를 갖도록 필드를 추가하는 대신
(그러면 SyntaxTree.h를 또 바꿔야 하니 다른 모듈과 재조율이 필요해진다),
아래처럼 그냥 `"this"`라는 고정 이름으로 **항상 동적 조회**하는 것으로
충분하다(this는 항상 메서드 호출 스코프의 최상단에 있어서 조회 비용이
낮음 - 정적 바인딩 최적화의 이점이 상대적으로 작다):

```cpp
expressionHandlers_[std::type_index(typeid(ThisExpression))] = [this](Expression*) {
    auto value = environment_.lookup("this");
    if (!value) {
        throw ExecutorError("클래스 외부에서 This를 사용했습니다.");
    }
    return *value;
};
```

(이 검사는 사실 Checker가 먼저 잡아야 정상이지만, 방어적으로 Executor에도
남겨둔다.)

`AssignExpression`의 target이 `FieldAccessExpression`인 경우 처리(기존
핸들러의 "아직 지원하지 않는 대입 대상입니다" 자리에 분기 추가):

```cpp
if (auto* fieldTarget = dynamic_cast<FieldAccessExpression*>(assign->target)) {
    Value object = evaluate(fieldTarget->object);
    if (!object.isInstance()) {
        throw ExecutorError("인스턴스가 아닌 대상에 필드를 대입했습니다.");
    }
    Value value = evaluate(assign->value);
    object.asInstance()->fields->define(fieldTarget->name.origin, value); // 없으면 새로 생성
    return value;
}
```

### 할 일 3: 정적 배열

Architecture.md §5.3을 그대로 구현한다.

```cpp
expressionHandlers_[std::type_index(typeid(ArrayExpression))] = [this](Expression* expr) {
    auto* arrayExpr = static_cast<ArrayExpression*>(expr);
    Value size = evaluate(arrayExpr->sizeExpr);
    if (!size.isNumber()) {
        throw ExecutorError("배열의 사이즈는 반드시 number여야 합니다.");
    }
    auto array = std::make_shared<ArrayValue>();
    array->items.resize(static_cast<size_t>(size.asNumber()));
    return Value(array);
};

expressionHandlers_[std::type_index(typeid(IndexExpression))] = [this](Expression* expr) {
    auto* index = static_cast<IndexExpression*>(expr);
    Value collection = evaluate(index->collection);
    if (!collection.isArray()) {
        throw ExecutorError("index 접근은 오직 배열만 지원합니다.");
    }
    Value idx = evaluate(index->index);
    if (!idx.isNumber()) {
        throw ExecutorError("인덱스는 반드시 숫자여야 합니다.");
    }
    auto i = static_cast<size_t>(idx.asNumber());
    auto& items = collection.asArray()->items;
    if (i >= items.size()) {
        throw ExecutorError("배열 인덱스 범위를 벗어났습니다.");
    }
    return items[i];
};
```

`AssignExpression`의 target이 `IndexExpression`인 경우도 위와 같은 검사 후
`items[i] = value;`.

### 할 일 4: instanceof

```cpp
expressionHandlers_[std::type_index(typeid(InstanceOfExpression))] = [this](Expression* expr) {
    auto* instOf = static_cast<InstanceOfExpression*>(expr);
    Value object = evaluate(instOf->object);
    if (!object.isInstance()) {
        return Value(false);
    }
    auto classValue = environment_.lookup(instOf->className.origin);
    if (!classValue || !classValue->isClass()) {
        throw ExecutorError("'{}'은(는) 클래스가 아닙니다.", instOf->className.origin);
    }
    return Value(object.asInstance()->klass == classValue->asClass());
};
```

### 할 일 5: import 실행

```cpp
statementHandlers_[std::type_index(typeid(ImportStatement))] = [this](Statement* stmt) {
    auto* importStmt = static_cast<ImportStatement*>(stmt);
    auto moduleScope = std::make_shared<Scope>();

    environment_.pushScope(); // declarations 실행용 임시 프레임
    for (Statement* decl : importStmt->declarations) {
        execute(decl);
    }
    // 방금 실행한 선언들이 현재(임시) 스코프에 등록되어 있다 - 이를
    // moduleScope로 복사해 옮긴다. Scope에 내용을 훑는 API가 없다면
    // 필요한 만큼만(예: 순회용 getter) 추가한다 - Environment/Scope도
    // 공유 인터페이스이니 추가하게 되면 팀에 공유한다.
    // (간단한 대안: declarations를 실행하기 전에 어떤 이름들이 선언될지
    // 미리 알고 있다면 그 이름들만 moduleScope로 복사해도 된다.)
    environment_.popScope();

    environment_.define(importStmt->alias.origin, Value(moduleScope));
};
```

`FieldAccessExpression`/`CallExpression`의 object가 `Module`이면
`instance->fields->get(...)` 대신 `moduleScope->get(...)`을 쓰도록 위 핸들러들에
분기를 추가한다(`object.isModule()`이면 `object.asModule()`이 그 `Scope`다).

### 테스트 가이드

기존 `ExecutorTest.cpp`/`ExecutorVariableTest.cpp`/`ExecutorControlFlowTest.cpp`와
같은 패턴(노드를 손으로 만들고 `executor.evaluate()`/`executor.execute()` 직접
호출, `std::ostringstream`으로 출력 검증)을 그대로 따른다. 함수 호출은 재귀
(`fact(5)`처럼)까지 반드시 테스트한다. 클래스는 필드 읽기/쓰기, 메서드 호출,
`init` 생성자, 존재하지 않는 필드/메서드 접근 시 `ExecutorError`까지 커버한다.

## 5. Shell 담당자 가이드

### 현재 상태

`RunPromptShell`은 전혀 바뀌지 않았다 - 그대로 REPL 모드로 쓴다. 새로
추가해야 하는 것은 파일 모드와 디버그 모드, 그리고 `main.cpp`의 CLI 분기다.
`ExecuteInterface`에 이미 `evaluate()`/`environment()`가 있고, `Executor`
(구체 클래스)에 `setStatementHook()`이 있다. `SyntaxNode`에
`containsLine(int)`가 있다(breakpoint 매칭용).

### 할 일 1: CommandLineArgs

```cpp
// Shell/CommandLineArgs.h
enum class ShellMode { Repl, Run, Debug };

struct CommandLineArgs {
    ShellMode mode = ShellMode::Repl;
    std::string path; // Run/Debug 모드일 때만 사용

    static CommandLineArgs parse(int argc, char** argv);
};
```

`argv[1]`이 `"run"`이면 `ShellMode::Run` + `argv[2]`를 path로, `"debug"`면
`ShellMode::Debug` + `argv[2]`, 인자가 없으면 `ShellMode::Repl`. 잘못된 조합
(예: `run`인데 경로 인자가 없음)은 예외를 던지거나 `Repl`로 폴백하는 등
팀에서 정책을 정한다.

### 할 일 2: FileRunMode

```cpp
class FileRunMode {
public:
    FileRunMode(TokenizeInterface& tokenizer, AssemblerInterface& assembler,
                CheckerInterface& checker, ExecuteInterface& executor);

    // 파일을 한 번에 읽어 tokenize -> assemble -> check -> execute를 한 번
    // 수행한다. 파일이 없거나 파이프라인 어디선가 예외가 나면 out에 오류
    // 메시지를 출력하고 false를 반환한다(RunPromptShell과 달리 계속 읽지
    // 않고 즉시 종료).
    bool run(const std::string& path, std::ostream& out);

private:
    TokenizeInterface& tokenizer_;
    AssemblerInterface& assembler_;
    CheckerInterface& checker_;
    ExecuteInterface& executor_;
};
```

파일을 읽는 방법은 Assembler 담당자가 이미 만든 `SourceReaderInterface`/
`FileSourceReader`(`Assembler/FileSourceReader.h`)를 그대로 재사용해도 되고,
`FileRunMode`가 직접 `std::ifstream`을 열어도 된다(둘 다 허용 - 재사용하면
Fake로 테스트하기 쉬워진다는 장점이 있다). 오류 발생 시
`SyntaxNode::getLine()`이나 예외 메시지에 이미 담긴 줄 번호를 그대로
출력한다(`RunPromptShell.cpp`가 예외를 처리하는 방식을 참고).

### 할 일 3: Debugger + DebugMode

```cpp
// Shell/Debugger.h
class Debugger {
public:
    Debugger(const ExecuteInterface& executor, std::istream& in, std::ostream& out);

    // Executor::setStatementHook에 그대로 넘길 수 있는 콜백.
    void onStatement(Statement* stmt);

private:
    enum class Mode { Step, Next, Continue };

    const ExecuteInterface& executor_;
    std::istream& in_;
    std::ostream& out_;
    std::set<int> breakpoints_;
    std::vector<std::string> watches_;
    Mode mode_ = Mode::Step;

    bool shouldStop(Statement* stmt) const;
    void printWatches();
    void promptAndHandleCommand(Statement* stmt);
};
```

`onStatement`는 매 statement마다 호출된다(Architecture.md §9.3).

```cpp
void Debugger::onStatement(Statement* stmt) {
    if (!shouldStop(stmt)) {
        return;
    }
    out_ << "[DEBUG] " << stmt->getLine() << "번째 줄에서 정지\n";
    printWatches();
    promptAndHandleCommand(stmt);
}

bool Debugger::shouldStop(Statement* stmt) const {
    if (mode_ == Mode::Step) return true;
    // Mode::Continue: breakpoint 줄에 해당할 때만 멈춘다. "break point는 step의
    // 반복으로 실행"된다는 요구사항은 이 함수가 매 statement(=매 step)마다
    // 호출된다는 사실 자체로 이미 만족된다 - 별도의 반복문이 필요 없다.
    for (int line : breakpoints_) {
        if (stmt->containsLine(line)) return true;
    }
    return false;
    // Mode::Next는 "직전에 멈췄던 깊이"를 기억해뒀다가 그 이하 깊이로
    // 돌아왔을 때만 true를 반환하도록 구현한다 - 깊이를 어떻게 셀지(콜스택
    // 깊이? 블록 중첩?)는 Executor 담당자와 상의해서 필요하면 StatementHook
    // 시그니처에 깊이 정보를 추가한다(예: void(Statement*, int depth)).
    // 시그니처를 바꾸면 Executor 담당자에게 반드시 공유한다.
}

void Debugger::printWatches() {
    for (const auto& name : watches_) {
        auto value = executor_.environment().lookup(name);
        out_ << "[WATCH] " << name << " = " << (value ? value->toString() : "undefined") << "\n";
    }
}
```

`promptAndHandleCommand`는 `in_`에서 한 줄 읽어 `step`/`next`/`break N`/
`breakpoints`/`remove N`/`continue`/`watch X`/`unwatch X`/`watches`/`inspect`를
파싱해 `mode_`/`breakpoints_`/`watches_`를 갱신한다(Command 패턴으로 각 명령을
별도 클래스로 뽑아도 되고, 간단한 if-else 체인으로 시작해도 된다). `inspect`는
`executor_.environment()`에 전체 스코프를 순회하는 API가 없다면 필요한 만큼
추가한다(`Environment`도 공유 인터페이스이니 추가하면 Executor 담당자에게
공유).

`DebugMode`는 `FileRunMode`와 비슷하지만 `ExecuteInterface&` 대신 구체
`Executor&`를 받는다(`setStatementHook`이 `ExecuteInterface`에는 없고
`Executor`에만 있기 때문 - Architecture.md §9.3에서 이미 이 트레이드오프를
명시했다).

```cpp
class DebugMode {
public:
    DebugMode(TokenizeInterface& tokenizer, AssemblerInterface& assembler,
              CheckerInterface& checker, Executor& executor);

    bool run(const std::string& path, std::istream& in, std::ostream& out);
};
```

내부에서 `Debugger debugger(executor, in, out); executor.setStatementHook(
[&debugger](Statement* stmt) { debugger.onStatement(stmt); });` 로 연결한 뒤
파이프라인을 한 번 실행한다.

### 할 일 4: main.cpp CLI 분기

```cpp
int main(int argc, char** argv) {
#ifdef _DEBUG
    testing::InitGoogleMock();
    return RUN_ALL_TESTS();
#else
    Tokenizer tokenizer;
    FileSourceReader sourceReader;
    Assembler assembler(tokenizer, sourceReader);
    Executor executor;
    Checker checker(executor);

    auto args = CommandLineArgs::parse(argc, argv);
    switch (args.mode) {
        case ShellMode::Repl: {
            RunPromptShell shell(tokenizer, assembler, checker, executor);
            shell.run(std::cin, std::cout);
            return 0;
        }
        case ShellMode::Run: {
            FileRunMode mode(tokenizer, assembler, checker, executor);
            return mode.run(args.path, std::cout) ? 0 : 1;
        }
        case ShellMode::Debug: {
            DebugMode mode(tokenizer, assembler, checker, executor);
            return mode.run(args.path, std::cin, std::cout) ? 0 : 1;
        }
    }
#endif
}
```

`main()`이 지금은 인자를 받지 않는데(`int main()`), CLI 분기를 추가하려면
`int main(int argc, char** argv)`로 시그니처를 바꿔야 한다 - 이건 Shell
모듈 내부 파일(`main.cpp`)만의 변경이라 다른 모듈과 조율할 필요는 없다.

### 테스트 가이드

`RunPromptShellTest.cpp`의 Mock 기반 테스트 패턴을 그대로 `FileRunModeTest.cpp`/
`DebugModeTest.cpp`에도 적용한다(4개 `*Interface`를 Mock으로 주입해 파이프라인
호출 순서/오류 처리를 검증). `Debugger`는 `ExecuteInterface`를 Mock/Fake로
주입하고, `std::istringstream`/`std::ostringstream`으로 명령 입력과 출력을
검증하는 단위 테스트를 작성한다(브레이크포인트 설정 → continue → 정확한 줄에서
멈추는지, watch 등록 → 출력에 값이 찍히는지 등).

## 6. 공통 체크리스트

작업을 마치기 전에 아래를 확인한다.

- [ ] `CodeFab.vcxproj`/`CodeFab.vcxproj.filters`에 새로 추가한 `.h`/`.cpp`
      파일을 등록했는가 (Visual Studio에서 파일 추가 시 자동으로 되지만,
      텍스트 에디터로 직접 추가했다면 수동으로 넣어야 한다).
- [ ] 빌드가 경고 없이(또는 기존 수준의 경고만) 통과하는가.
- [ ] 기존 133개 테스트가 여전히 전부 통과하는가(`x64\Debug\CodeFab.exe`
      실행, Debug 구성은 `_DEBUG`가 정의되어 `main()`이 자동으로 전체 테스트를
      돌린다).
- [ ] 새로 만든 기능에 대한 단위 테스트를 추가했는가.
- [ ] 교차 모듈 계약(클래스 이름/필드 이름/생성자 시그니처)을 바꿨다면, 그
      사실을 팀에 공유하고 이 문서(`Implement.md`)도 함께 갱신했는가.
