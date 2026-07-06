#pragma once
#include <string>

enum class TokenType {
    // 리터럴
    NUMBER,
    STRING,
    TRUE,
    FALSE,

    // 키워드
    VAR,
    PRINT,
    IF,
    ELSE,
    FOR,

    // 식별자
    IDENTIFIER,

    // 산술 연산자
    PLUS,           // +
    MINUS,          // -
    STAR,           // *
    SLASH,          // /

    // 대입 / 비교 연산자
    EQUAL,          // =
    EQUAL_EQUAL,    // ==
    BANG,           // !
    BANG_EQUAL,     // !=
    LESS,           // <
    LESS_EQUAL,     // <=
    GREATER,        // >
    GREATER_EQUAL,  // >=

    // 구분자
    SEMICOLON,      // ;
    LEFT_PAREN,     // (
    RIGHT_PAREN,    // )
    LEFT_BRACE,     // {
    RIGHT_BRACE,    // }

    END_OF_FILE
};

struct Token {
    TokenType   type;
    std::string origin; // 원본 문자열 (STRING은 따옴표 제거된 값)
    int         line;
};
