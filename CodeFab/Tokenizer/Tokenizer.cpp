#include "Tokenizer.h"
#include <cctype>
#include <unordered_map>

static const std::unordered_map<char, TokenType> SINGLE_CHAR = {
    { '(', TokenType::LEFT_PAREN    }, { ')', TokenType::RIGHT_PAREN  },
    { '{', TokenType::LEFT_BRACE    }, { '}', TokenType::RIGHT_BRACE  },
    { '[', TokenType::LEFT_BRACKET  }, { ']', TokenType::RIGHT_BRACKET },
    { ';', TokenType::SEMICOLON     }, { '.', TokenType::DOT          },
    { ',', TokenType::COMMA         }, { ':', TokenType::COLON        },
    { '+', TokenType::PLUS          }, { '-', TokenType::MINUS        },
    { '*', TokenType::STAR          }, { '/', TokenType::SLASH        },
    { '%', TokenType::PERCENT       },
};

void Tokenizer::reset(const std::string& src) {
    source  = src;
    start   = 0;
    current = 0;
    line    = 1;
    tokens.clear();
}

std::vector<Token> Tokenizer::tokenize(const std::string& src) {
    reset(src);

    while (!isAtEnd()) {
        start = current;
        scanToken();
    }

    return tokens;
}

bool Tokenizer::isAtEnd() const {
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

char Tokenizer::peek() const {
    return isAtEnd() ? '\0' : source[current];
}

char Tokenizer::peekNext() const {
    if (current + 1 >= static_cast<int>(source.size())) return '\0';
    return source[current + 1];
}

void Tokenizer::addToken(TokenType type) {
    tokens.push_back({ type, source.substr(start, current - start), line });
}

void Tokenizer::addToken(TokenType type, std::string value) {
    tokens.push_back({ type, std::move(value), line });
}

bool Tokenizer::isDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c));
}

bool Tokenizer::isAlpha(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool Tokenizer::isAlphaNumeric(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

void Tokenizer::scanToken() {
    char c = advance();

    if (auto it = SINGLE_CHAR.find(c); it != SINGLE_CHAR.end()) {
        addToken(it->second);
        return;
    }

    switch (c) {
        case '=': addToken(match('=') ? TokenType::EQUAL_EQUAL   : TokenType::EQUAL);   break;
        case '!': addToken(match('=') ? TokenType::BANG_EQUAL    : TokenType::BANG);    break;
        case '<': addToken(match('=') ? TokenType::LESS_EQUAL    : TokenType::LESS);    break;
        case '>': addToken(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER); break;
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
            scanDefault(c);
            break;
    }
}

void Tokenizer::scanString() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') line++;
        advance();
    }
    if (isAtEnd())
        throw AssemblyError("[line {}] unterminated string literal.", line);

    advance(); // 닫는 '"' 소비
    addToken(TokenType::STRING, source.substr(start + 1, current - start - 2));
}

void Tokenizer::scanDefault(char c) {
    if (isDigit(c))      scanNumber();
    else if (isAlpha(c)) scanIdentifier();
    else throw AssemblyError("[line {}] unknown character: '{}'", line, c);
}

void Tokenizer::scanNumber() {
    while (isDigit(peek())) advance();

    if (peek() == '.' && isDigit(peekNext())) {
        advance();
        while (isDigit(peek())) advance();
    }
    addToken(TokenType::NUMBER);
}

void Tokenizer::scanIdentifier() {
    while (isAlphaNumeric(peek())) advance();

    std::string text = source.substr(start, current - start);
    auto it = KEYWORDS.find(text);
    addToken(it != KEYWORDS.end() ? it->second : TokenType::IDENTIFIER);
}
