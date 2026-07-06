#include "Tokenizer.h"
#include <stdexcept>
#include <cctype>
#include <unordered_map>

static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    { "var",   TokenType::VAR   },
    { "print", TokenType::PRINT },
    { "if",    TokenType::IF    },
    { "else",  TokenType::ELSE  },
    { "for",   TokenType::FOR   },
    { "true",  TokenType::TRUE  },
    { "false", TokenType::FALSE },
};

std::vector<Token> Tokenizer::tokenize(const std::string& src) {
    source  = src;
    start   = 0;
    current = 0;
    line    = 1;
    tokens.clear();

    while (!isAtEnd()) {
        start = current;
        scanToken();
    }
    tokens.push_back({ TokenType::END_OF_FILE, "", line });
    return tokens;
}

bool Tokenizer::isAtEnd() {
    return current >= static_cast<int>(source.size());
}

char Tokenizer::advance() {
    return source[current++];
}

bool Tokenizer::match(char expected) {
    if (isAtEnd() || source[current] != expected) return false;
    current++;
    return true;
}

char Tokenizer::peek() {
    return isAtEnd() ? '\0' : source[current];
}

char Tokenizer::peekNext() {
    if (current + 1 >= static_cast<int>(source.size())) return '\0';
    return source[current + 1];
}

void Tokenizer::addToken(TokenType type) {
    tokens.push_back({ type, source.substr(start, current - start), line });
}

void Tokenizer::scanToken() {
    char c = advance();
    switch (c) {
        case '(': addToken(TokenType::LEFT_PAREN);  break;
        case ')': addToken(TokenType::RIGHT_PAREN); break;
        case '{': addToken(TokenType::LEFT_BRACE);  break;
        case '}': addToken(TokenType::RIGHT_BRACE); break;
        case ';': addToken(TokenType::SEMICOLON);   break;
        case '+': addToken(TokenType::PLUS);        break;
        case '-': addToken(TokenType::MINUS);       break;
        case '*': addToken(TokenType::STAR);        break;
        case '/': addToken(TokenType::SLASH);       break;
        case '=': addToken(match('=') ? TokenType::EQUAL_EQUAL   : TokenType::EQUAL);          break;
        case '!': addToken(match('=') ? TokenType::BANG_EQUAL    : TokenType::BANG);           break;
        case '<': addToken(match('=') ? TokenType::LESS_EQUAL    : TokenType::LESS);           break;
        case '>': addToken(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);        break;
        case ' ':
        case '\r':
        case '\t':
            break;
        case '\n':
            line++;
            break;
        case '"':
            scanString();
            break;
        default:
            if (std::isdigit(static_cast<unsigned char>(c))) {
                scanNumber();
            } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                scanIdentifier();
            } else {
                throw std::runtime_error(std::string("알 수 없는 문자: '") + c + "'");
            }
            break;
    }
}

void Tokenizer::scanString() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') line++;
        advance();
    }
    if (isAtEnd())
        throw std::runtime_error("문자열이 종결되지 않았습니다.");

    advance(); // 닫는 '"' 소비
    // 따옴표를 제거한 실제 문자열 값을 origin에 저장
    std::string value = source.substr(start + 1, current - start - 2);
    tokens.push_back({ TokenType::STRING, value, line });
}

void Tokenizer::scanNumber() {
    while (std::isdigit(static_cast<unsigned char>(peek()))) advance();

    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        advance(); // '.' 소비
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
    }
    addToken(TokenType::NUMBER);
}

void Tokenizer::scanIdentifier() {
    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')
        advance();

    std::string text = source.substr(start, current - start);
    auto it = KEYWORDS.find(text);
    addToken(it != KEYWORDS.end() ? it->second : TokenType::IDENTIFIER);
}
