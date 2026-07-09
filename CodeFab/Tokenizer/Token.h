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

    // 키워드 (확장: function/class/array/import/instanceof/상속)
    FUNC,           // Func
    RETURN,         // return
    CLASS,          // Class
    THIS,           // This
    ARRAY,          // Array
    IMPORT,         // import
    ALIAS,          // alias
    INSTANCEOF,     // instanceof

    SUPER,          // Super

    // 식별자
    IDENTIFIER,

    // 산술 연산자
    PLUS,           // +
    MINUS,          // -
    STAR,           // *
    SLASH,          // /
    PERCENT,        // %

    // 논리 연산자
    AND,            // and
    OR,             // or

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

    // 구분자 (확장: class/array/function/상속)
    DOT,            // .
    LEFT_BRACKET,   // [
    RIGHT_BRACKET,  // ]
    COMMA,          // ,

    COLON,          // :
};

struct Token {
    TokenType   type;
    std::string origin; // 원본 문자열 (STRING은 따옴표 제거된 값)
    int         line;

    bool operator==(const Token& op) const {
        return type == op.type && origin == op.origin && line == op.line;
    }
};
