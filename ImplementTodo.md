# CodeFab TODO 작업 분담 가이드

> 이 문서는 `TODO.md`에 쌓인 "아직 손대지 않은 빈틈"을, 기존 `Implement.md`와 같은 방식으로
> **Tokenizer/Assembler/Checker/Executor/Shell 5명이 병렬로 나눠서 작업**할 수 있도록 정리한다.
>
> - 기존 모듈 담당자 배정을 그대로 유지한다(자기 모듈 관련 작업을 우선 배정).
> - **Shell 담당자는 아직 Debug Mode 구현이 끝나지 않았으므로**, 이번 라운드에서는
>   본인 작업과 바로 이어지는 항목 2개만 배정하고 새 기능/리팩토링은 배정하지 않는다.
> - 여러 모듈에 걸친 작업(상속 등)은 모듈별로 쪼개서 해당 모듈 담당자에게 나눠 배정했다.
> - **교차 모듈 계약(Token, SyntaxTree 노드, `*Interface.h`)을 바꿔야 하면 반드시 다른
>   담당자와 먼저 상의한다** - Implement.md 원칙과 동일하다.
> - PR 충돌을 줄이기 위해, 아래 작업이 공통으로 의존하는 인터페이스 일부는 이미
>   코드에 반영해뒀다(§0 참고). 각자 이 계약 위에서 구현/테스트만 채우면 된다.

## 0. 미리 반영해 둔 공용 계약 (커밋 완료, 건드릴 필요 없음)

상속(Super, `:`) 작업이 Assembler/Checker/Executor 세 담당자에게 걸쳐 있어서, 세 사람이
각자 같은 구조를 살짝 다르게 만들면 그 자체로 머지 충돌이 난다. 그래서 아래 두 가지
**타입 선언만** 미리 커밋해뒀다(파싱/의미검사/실행 로직은 전혀 없음 - 각 담당자가 채워
넣는다).

- `Tokenizer/Token.h`: `TokenType`에 `SUPER`, `COLON` 값 추가(Tokenizer는 아직 이 토큰을
  스캔하지 않는다 - §1 할 일).
- `Assembler/SyntaxTree.h`:
  - `SuperExpression` 노드 추가(필드 없음, `ThisExpression`과 동일한 모양). `Super.move(3)`은
    별도 전용 노드로 만들지 않고, 기존 postfix 체인이 `FieldAccessExpression(object=
    SuperExpression, name="move")`/`CallExpression`으로 조립하도록 설계했다(This가
    `IdentifierExpression`과 같은 경로를 타는 것과 동일한 재사용 원칙).
  - `ClassDeclareStatement`에 `IdentifierExpression* const superclass`(기본값 `nullptr`)
    필드 추가. 기존 3개 인자 생성 코드(`Assembler.cpp`, 각 테스트)는 그대로 컴파일된다.

이 두 가지 외의 상속 관련 결정(문법 세부사항, `Super.field` 허용 여부, 의미검사 규칙,
실행 규칙)은 전혀 확정하지 않았다 - 아래 각 담당자 절에서 채운다.

## 1. Tokenizer 담당자

Tokenizer 자체의 신규 기능 TODO는 없다. 대신 상속 토큰 스캔(작지만 다른 모든 작업의
전제조건)과, 모듈 하나에 종속되지 않는 통합 테스트/문서 작업을 배정한다.

- [ ] **상속 키워드/구분자 스캔**: `Tokenizer.cpp`의 `KEYWORDS` 맵에 `{"Super", TokenType::SUPER}`
      추가, `scanToken()`에 `case ':': addToken(TokenType::COLON); break;` 추가.
      `TokenizerTest.cpp`에 각각 최소 1개 테스트 추가(기존 §1 패턴 그대로).
- [ ] **TODO.md #1 문서 갱신**: `Implement.md` §5(Executor, "할 일 5: import 실행") 의사코드를
      실제 `Executor.cpp` 구현(ScopeGuard 안에서 실행 직후 `moduleScope->define`으로 즉시
      복사)에 맞게 갱신. `Architecture.md` §7.3에도 동일 내용 반영.
- [ ] **TODO.md #2 문서 보강**: Module이 Class와 달리 "필드/메서드 이중 컨테이너"가 필요
      없다는 점을 `Architecture.md` §7.3 / `Implement.md` §5에 명시(TODO.md #2에 초안 문구 있음).
- [ ] **Integration test - Function (§1)**: `RunPromptShellIntegrationTest`에 함수 선언/호출/
      재귀/return-없음/파라미터 값 전달/각종 에러 시나리오 추가(TODO.md "# Integration test"
      §1 목록 그대로).
- [ ] **Integration test - 정적 배열 (§3)**: 생성/인덱스 read-write/기본값/각종 런타임 에러
      시나리오 추가(TODO.md §3 목록).
- [ ] **Integration test - instanceof (§5)**: true/false 케이스 추가(TODO.md §5 목록, 상속
      전 단계 범위만 - 상속 관련 instanceof는 5번 항목이 끝난 뒤 별도로 추가됨에 유의).

## 2. Assembler 담당자

- [ ] **TODO.md #8 (버그 수정)**: `Assembler.cpp`의 `makeBinaryExpression`에서 `default:`
      분기가 암묵적으로 `OrExpression`을 반환하는 문제. `case TokenType::OR:`를 명시적으로
      추가하고, `default:`는 `assert(false)`/`AssemblerError`로 바꿔 우선순위 테이블과
      `switch`가 어긋나면 바로 드러나게 한다.
- [ ] **TODO.md #9 (결정 + 구현, Assembler 쪽)**: import 대상 파일에서 `ClassDeclareStatement`를
      허용할지 팀 결정을 주도한다(Checker/Executor 담당자와 상의). 허용하기로 하면
      `Assembler.cpp`의 `parseImport` 허용 목록에 `ClassDeclareStatement` 추가하고
      `AssemblerImportTest`에 회귀 테스트 추가. 허용하지 않기로 하면 그 결정을
      `Architecture.md` §7.x에 명시하고 Executor 담당자에게 죽은 코드 제거를 요청한다.
- [ ] **상속 - Assembler 파싱**: §0에서 추가된 `SUPER`/`COLON` 토큰과 `SuperExpression`/
      `ClassDeclareStatement.superclass`를 채운다.
      - `classDeclStmt -> CLASS IDENTIFIER (COLON IDENTIFIER)? LEFT_BRACE methodDeclStmt* RIGHT_BRACE`
        파싱 - `COLON`이 있으면 다음 `IDENTIFIER`를 `IdentifierExpression`으로 만들어
        `superclass`에 넣는다.
      - `primary()`에 `SUPER` 분기 추가(`This` 분기와 동일한 패턴) → `SuperExpression` 생성.
        `Super.move(3)`은 기존 postfix 체인이 알아서 `FieldAccessExpression`/`CallExpression`으로
        조립하므로 추가 파싱 로직 불필요.
      - `Super.field`(메서드 호출이 아닌 필드 접근)를 문법적으로 허용할지는 Checker 담당자와
        상의해서 결정(파서 입장에서는 이미 postfix 체인이 둘 다 만들 수 있으므로, 제한하려면
        Checker가 의미 검사로 막는 편을 권장).
      - `AssemblerTest.cpp`에 `Class B : A { }` 파싱 결과, `Super.move(3)` 파싱 결과 골든
        트리 테스트 추가.
- [ ] **TODO.md #3**: `import "sum.txt" alias sum; sum.func();`처럼 import + `.` 접근을
      한 입력 안에서 함께 검증하는 `AssemblerTest.cpp`(또는 `AssemblerImportTest`) 테스트 추가.

## 3. Checker 담당자

- [ ] **상속 - Checker 의미 검사**:
      - 자기 자신 상속(`Class A : A`) 금지, 클래스가 아닌 대상 상속 금지(`superclass`가
        가리키는 이름이 스코프에서 `Class` 선언으로 확인되지 않으면 오류 - 단, 이 판정이
        런타임에만 가능한 부분은 Executor 오류로 남겨도 됨, Function 검사 때의 원칙과 동일).
      - 클래스 외부에서 `SuperExpression` 사용 금지, **부모 없는 클래스**(`superclass ==
        nullptr`)의 메서드 안에서 `Super` 사용 금지.
      - (Assembler 담당자와 상의해서 결정된) `Super.field` 허용 여부를 Checker가 강제해야
        한다면 여기서 구현.
- [ ] **TODO.md 코드 정리 #3의 Checker 부분 (PR #36 지적사항)**: `Checker.cpp`의
      `checkImport`가 `enterScope()`/`exitScope()` 없이 현재 스코프에서 바로
      `checkStatement(decl)`을 호출해서 import 내부 선언 이름이 바깥 스코프로 새어나가는
      버그. `checkBlock`/`checkFor`와 동일한 `enterScope()` + `try { ... } catch { exitScope();
      throw; }` 패턴을 적용한다. **이 수정은 Executor 담당자가 하는 `lookupAt`/`assignAt`
      범위 검사 작업의 전제 조건이므로 먼저 끝내고 공유한다.**
- [ ] **TODO.md #10 (결정 + 설계 리드)**: `Checker`/`Optimizer`(상수 폴딩) 책임 분리를
      팀 논의로 확정한다. 분리 시점(이번 스프린트 vs 이후)과 `Optimizer`가
      `ExecutorInterface`에 의존하는 방식을 Architecture.md에 정리. 실제 분리 리팩토링
      착수 여부는 팀 결정에 따르며, 이번 라운드에서는 "결정 + 설계 문서화"까지를 목표로 한다.
- [ ] **TODO.md #9 관련**: Assembler 담당자의 "클래스 import 허용" 결정에 맞춰, import된
      `ClassDeclareStatement`에 대한 의미 검사(있다면)를 함께 정리.

## 4. Executor 담당자

- [ ] **상속 - Executor 실행 규칙**:
      - 메서드 탐색을 `klass->methods` → 없으면 `klass->superclass->methods`로 재귀
        확장(§코드 정리 #4의 `findMethod` 헬퍼를 만들 때 이 체인 탐색까지 함께 구현하면
        일석이조).
      - `callMethod`가 callee의 object를 평가하기 전에 그 object 표현식이
        `SuperExpression`인지 먼저 확인 - 맞으면 **현재 인스턴스(this)는 그대로 유지한 채**
        메서드 탐색 시작점만 현재 클래스가 아니라 `superclass`로 강제 이동.
      - `InstanceOfExpression` 평가를 "정확히 같은 클래스인지"(포인터 비교 1회)에서
        "`instance.klass`부터 `superclass` 체인을 따라 올라가며 일치하는 게 있는지"로 확장.
- [ ] **TODO.md 코드 정리 #3의 Executor 부분**: `Environment::lookupAt`/`assignAt`
      (`Environment.cpp`)에 `distance >= scopes_.size()`면 예외를 던지는 방어 코드 추가,
      `scopes_[index]`를 `scopes_.at(index)`로 변경. **Checker 담당자의 `checkImport` 스코프
      수정이 먼저 반영됐는지 확인한 뒤 작업한다**(그 전에는 이 방어 코드가 진짜 버그를
      가려버릴 수 있음).
- [ ] **TODO.md 코드 정리 #4**: `instantiate`(`init` 탐색)와 `callMethod`(임의 메서드 탐색)의
      중복된 선형 탐색을 `MethodDeclareStatement* findMethod(const ClassDeclareStatement*,
      const std::string&)` 헬퍼로 통합(위 상속 작업과 함께 하면 `superclass` 체인까지 한
      곳에서만 고치면 됨).
- [ ] **TODO.md 코드 정리 #5**: `CallExpression`/`callMethod`(모듈 분기)/`callMethod`(인스턴스
      분기) 세 곳에 중복된 `for (Expression* arg : argExprs) args.push_back(evaluate(arg));`
      패턴을 `std::vector<Value> evaluateArgs(const std::vector<Expression*>&)` 헬퍼로 통합.
- [ ] **TODO.md #12**: `IntegrationTest`에 클래스 메서드 재귀 호출(`This.method(...)`) 테스트
      추가 - 재귀 중 `this`가 항상 같은 인스턴스를 가리키는지 확인하는 케이스 포함.
- [ ] **TODO.md #13**: `IntegrationTest`에 module import end-to-end 테스트 추가(함수/변수
      접근, 블록 스코프별 독립성, 여러 module 동시 import 시 alias 충돌 없음 등 - TODO.md
      #13 체크리스트 그대로). Assembler 담당자의 "클래스 import 허용" 결정이 나기 전까지는
      `Func`/`var`만 있는 module로 우선 진행.
- [ ] **TODO.md #11 (선택, 여유가 되면)**: RTTI(`typeid`/`dynamic_cast`) 기반 디스패치를
      Visitor 패턴으로 리팩토링. 이 작업은 `SyntaxTree.h`(Assembler 소유 파일)에
      `accept()`/`SyntaxNodeVisitor`를 새로 추가해야 하므로, **착수 전에 반드시 Assembler
      담당자와 조율**한다(이번 라운드 필수 항목은 아님 - 다른 작업 다 끝나면 진행).

## 5. Shell 담당자 (Debug Mode 작업 중 - 추가 배정 최소화)

Debug Mode(`DebugMode.h/.cpp`, `Debugger.h/.cpp`)가 아직 진행 중이므로, 새 기능이나 다른
모듈 작업은 배정하지 않는다. 대신 지금 하고 있는 작업과 바로 이어지는 항목 2개만 맡는다.

- [ ] **TODO.md 코드 정리 #6**: `main.cpp`의 `switch(args.mode)`가 Debug Mode 완성 시 계속
      커질 구조이므로, `RunPromptShell`/`FileRunMode`/(완성될) `DebugMode`가 공유할 실행
      인터페이스(가칭 `ShellRunner`, `int run()` 형태)를 설계하고 세 클래스가 이를 구현하도록
      정리. `main.cpp`의 `switch`는 "모드에 맞는 구현체 생성 후 `run()` 호출"로 단순화.
      **Debug Mode 구현이 이 인터페이스 위에서 완성되도록, 인터페이스 정리를 Debug Mode
      마무리보다 먼저 하는 것을 권장**(나중에 하면 `DebugMode`도 다시 고쳐야 함).
- [ ] **TODO.md "# Integration test" §7**: Debug Mode가 완성되는 대로, 파일 모드 정상 실행/
      파일 없음/실행 중 오류 즉시 종료, 디버그 모드 breakpoint/step-next/watch/inspect
      시나리오를 실제 4-Unit을 붙인 통합 테스트로 추가.

## 6. 공통 체크리스트 (Implement.md §6과 동일)

- [ ] `CodeFab.vcxproj`/`CodeFab.vcxproj.filters`에 새 파일을 등록했는가.
- [ ] 기존 133개 테스트 + 새로 추가되는 테스트가 전부 통과하는가.
- [ ] 교차 모듈 계약(Token, SyntaxTree 노드, `*Interface.h`, 생성자 시그니처)을 바꿨다면
      팀에 공유하고 `Implement.md`/`ImplementTodo.md`도 함께 갱신했는가.
- [ ] 에러 메시지(한글/영어 혼재, TODO.md 코드 정리 #1)와 주석(TODO.md 코드 정리 #2)은
      각자 자기 담당 모듈 파일을 손대는 김에 정리한다 - 별도로 배정하지 않고 각 작업에
      자연스럽게 포함시킨다(다른 사람 파일까지 건드리면 오히려 충돌 위험이 커짐).
