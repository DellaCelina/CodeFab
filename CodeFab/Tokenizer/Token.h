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

    // 키워드 (3일차 확장: function/class/array/import/instanceof)
    // Tokenizer는 아직 이 토큰들을 만들어내지 않는다 - Implement.md의 Tokenizer
    // 담당자 안내를 참고해 키워드 인식(KEYWORDS 맵 등)에 추가해야 한다.
    FUNC,           // Func
    RETURN,         // return
    CLASS,          // Class
    THIS,           // This
    ARRAY,          // Array
    IMPORT,         // import
    ALIAS,          // alias
    INSTANCEOF,     // instanceof

    // 키워드 (상속 확장: Architecture.md §4.5 / TODO.md #5)
    // Tokenizer는 아직 이 토큰들을 만들어내지 않는다 - ImplementTodo.md의 상속
    // 담당 파트를 참고해 키워드/구분자 인식에 추가해야 한다.
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

    // 구분자 (3일차 확장: class/array/function)
    DOT,            // .
    LEFT_BRACKET,   // [
    RIGHT_BRACKET,  // ]
    COMMA,          // ,

    // 구분자 (상속 확장: Architecture.md §4.5 / TODO.md #5)
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
