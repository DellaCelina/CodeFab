#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Tokenizer.h"

using ::testing::ElementsAre;

class TokenizerFixture : public ::testing::Test {
protected:
    Tokenizer tokenizer;

    std::vector<TokenType> getTypes(const std::vector<Token>& tokens) {
        std::vector<TokenType> result;
        for (const auto& tok : tokens)
            result.push_back(tok.type);
        return result;
    }
};

TEST_F(TokenizerFixture, TokenTypes) {
    auto tokens = tokenizer.tokenize("print 1 + 2 * 3;");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::PRINT,
        TokenType::NUMBER,
        TokenType::PLUS,
        TokenType::NUMBER,
        TokenType::STAR,
        TokenType::NUMBER,
        TokenType::SEMICOLON
    ));
}

TEST_F(TokenizerFixture, TokenOrigins) {
    auto tokens = tokenizer.tokenize("print 1 + 2 * 3;");
    EXPECT_EQ(tokens[0].origin, "print");
    EXPECT_EQ(tokens[1].origin, "1");
    EXPECT_EQ(tokens[2].origin, "+");
    EXPECT_EQ(tokens[3].origin, "2");
    EXPECT_EQ(tokens[4].origin, "*");
    EXPECT_EQ(tokens[5].origin, "3");
    EXPECT_EQ(tokens[6].origin, ";");
}

TEST_F(TokenizerFixture, TokenCount) {
    auto tokens = tokenizer.tokenize("print 1 + 2 * 3;");
    EXPECT_EQ(tokens.size(), 7u);
}

// 빈 입력
TEST_F(TokenizerFixture, EmptyInput) {
    auto tokens = tokenizer.tokenize("");
    EXPECT_EQ(tokens.size(), 0u);
}

// 키워드
TEST_F(TokenizerFixture, Keywords) {
    auto tokens = tokenizer.tokenize("var if else for");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::VAR, TokenType::IF, TokenType::ELSE, TokenType::FOR
    ));
}

// 식별자 (키워드와 구분)
TEST_F(TokenizerFixture, Identifier) {
    auto tokens = tokenizer.tokenize("variable");
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].origin, "variable");
}

// 불리언 리터럴
TEST_F(TokenizerFixture, BooleanLiterals) {
    auto tokens = tokenizer.tokenize("true false");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::TRUE, TokenType::FALSE
    ));
}

// 문자열 리터럴 (따옴표 제거 확인)
TEST_F(TokenizerFixture, StringLiteral) {
    auto tokens = tokenizer.tokenize("\"hello\"");
    EXPECT_EQ(tokens[0].type, TokenType::STRING);
    EXPECT_EQ(tokens[0].origin, "hello");
}

// 소수점 숫자
TEST_F(TokenizerFixture, FloatNumber) {
    auto tokens = tokenizer.tokenize("3.14");
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].origin, "3.14");
}

// 이중 문자 연산자
TEST_F(TokenizerFixture, TwoCharOperators) {
    auto tokens = tokenizer.tokenize("== != <= >=");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL,
        TokenType::LESS_EQUAL,  TokenType::GREATER_EQUAL
    ));
}

// 줄 번호 추적
TEST_F(TokenizerFixture, LineTracking) {
    auto tokens = tokenizer.tokenize("var x;\nvar y;");
    EXPECT_EQ(tokens[0].line, 1); // var
    EXPECT_EQ(tokens[3].line, 2); // var (두 번째 줄)
}

// 종결되지 않은 문자열 예외
TEST_F(TokenizerFixture, UnterminatedString) {
    // 문자열이 닫히지 않은 채 입력이 끝나면 IncompleteInputError를 던진다
    // (TokenizeInterface.h/Tokenizer.cpp의 scanString() 참고). IncompleteInputError는
    // std::runtime_error가 아니라 std::exception을 직접 상속한다.
    EXPECT_THROW(tokenizer.tokenize("\"hello"), IncompleteInputError);
}

// 알 수 없는 문자 예외
TEST_F(TokenizerFixture, UnknownCharacter) {
    // 인식 불가능한 문자를 만나면 AssemblyError를 던진다. AssemblyError도
    // std::runtime_error가 아니라 std::exception을 직접 상속한다.
    EXPECT_THROW(tokenizer.tokenize("@"), AssemblyError);
}

// 닫히지 않은 괄호: Tokenizer는 에러 없이 토큰을 반환, 구조 오류는 Assembler가 처리
TEST_F(TokenizerFixture, UnclosedParen_DoesNotThrow) {
    EXPECT_NO_THROW(tokenizer.tokenize("print (1 + 2;"));
}

// 닫히지 않은 중괄호: Tokenizer는 에러 없이 토큰을 반환, 구조 오류는 Assembler가 처리
TEST_F(TokenizerFixture, UnclosedBrace_DoesNotThrow) {
    EXPECT_NO_THROW(tokenizer.tokenize("{ var a = 1;"));
}

// var 선언 구문
TEST_F(TokenizerFixture, VarDeclaration) {
    auto tokens = tokenizer.tokenize("var x = 10;");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::VAR, TokenType::IDENTIFIER,
        TokenType::EQUAL, TokenType::NUMBER,
        TokenType::SEMICOLON
    ));
}

// 괄호 및 중괄호
TEST_F(TokenizerFixture, ParenAndBrace) {
    auto tokens = tokenizer.tokenize("( ) { }");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::LEFT_PAREN,  TokenType::RIGHT_PAREN,
        TokenType::LEFT_BRACE,  TokenType::RIGHT_BRACE
    ));
}

// 빼기 및 나누기 연산자
TEST_F(TokenizerFixture, MinusAndSlash) {
    auto tokens = tokenizer.tokenize("10 - 2 / 5");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::NUMBER, TokenType::MINUS,
        TokenType::NUMBER, TokenType::SLASH,
        TokenType::NUMBER
    ));
}

// 3일차 신규 키워드
TEST_F(TokenizerFixture, NewKeywords) {
    auto tokens = tokenizer.tokenize("Func return Class This Array import alias instanceof");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::FUNC, TokenType::RETURN, TokenType::CLASS, TokenType::THIS,
        TokenType::ARRAY, TokenType::IMPORT, TokenType::ALIAS, TokenType::INSTANCEOF
    ));
}

// 새 키워드 origin 확인
TEST_F(TokenizerFixture, NewKeywordOrigins) {
    auto tokens = tokenizer.tokenize("Func Class This Array");
    EXPECT_EQ(tokens[0].origin, "Func");
    EXPECT_EQ(tokens[1].origin, "Class");
    EXPECT_EQ(tokens[2].origin, "This");
    EXPECT_EQ(tokens[3].origin, "Array");
}

// 새 구분자
TEST_F(TokenizerFixture, NewDelimiters) {
    auto tokens = tokenizer.tokenize(". [ ] ,");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::DOT, TokenType::LEFT_BRACKET, TokenType::RIGHT_BRACKET, TokenType::COMMA
    ));
}

// 필드 접근 패턴: r.speed
TEST_F(TokenizerFixture, DotFieldAccess) {
    auto tokens = tokenizer.tokenize("r.speed");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::IDENTIFIER, TokenType::DOT, TokenType::IDENTIFIER
    ));
}

// 배열 인덱스 패턴: arr[0]
TEST_F(TokenizerFixture, ArrayIndex) {
    auto tokens = tokenizer.tokenize("arr[0]");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::IDENTIFIER, TokenType::LEFT_BRACKET, TokenType::NUMBER, TokenType::RIGHT_BRACKET
    ));
}

// 함수 호출 파라미터: add(a, b)
TEST_F(TokenizerFixture, FunctionCallWithComma) {
    auto tokens = tokenizer.tokenize("add(a, b)");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::IDENTIFIER, TokenType::LEFT_PAREN, TokenType::IDENTIFIER,
        TokenType::COMMA, TokenType::IDENTIFIER, TokenType::RIGHT_PAREN
    ));
}

// 소수점과 DOT 충돌 없음: 3.14는 NUMBER, r.x는 IDENTIFIER DOT IDENTIFIER
TEST_F(TokenizerFixture, DotDoesNotConflictWithFloat) {
    auto tokens = tokenizer.tokenize("3.14 r.x");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::NUMBER, TokenType::IDENTIFIER, TokenType::DOT, TokenType::IDENTIFIER
    ));
    EXPECT_EQ(tokens[0].origin, "3.14");
}

// Array는 예약어 — 변수명으로 쓰면 ARRAY 토큰
TEST_F(TokenizerFixture, ArrayIsReservedKeyword) {
    auto tokens = tokenizer.tokenize("Array");
    EXPECT_EQ(tokens[0].type, TokenType::ARRAY);
}

// 논리 연산자 키워드
TEST_F(TokenizerFixture, AndOrKeywords) {
    auto tokens = tokenizer.tokenize("and or");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::AND, TokenType::OR
    ));
}

// and/or origin 확인
TEST_F(TokenizerFixture, AndOrOrigins) {
    auto tokens = tokenizer.tokenize("and or");
    EXPECT_EQ(tokens[0].origin, "and");
    EXPECT_EQ(tokens[1].origin, "or");
}

// 나머지 연산자
TEST_F(TokenizerFixture, PercentOperator) {
    auto tokens = tokenizer.tokenize("10 % 3");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::NUMBER, TokenType::PERCENT, TokenType::NUMBER
    ));
    EXPECT_EQ(tokens[1].origin, "%");
}

// 상속 키워드 Super
TEST_F(TokenizerFixture, SuperKeyword_IsRecognized) {
    auto tokens = tokenizer.tokenize("Super");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::SUPER);
    EXPECT_EQ(tokens[0].origin, "Super");
}

// 상속 구분자 콜론
TEST_F(TokenizerFixture, ColonSymbol_IsRecognized) {
    auto tokens = tokenizer.tokenize(":");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::COLON);
    EXPECT_EQ(tokens[0].origin, ":");
}