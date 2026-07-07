#include "Tokenizer.h"
#include "ShellErrors.h"
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


void Tokenizer::reset(const std::string& src) {
    source     = src;
    start      = 0;
    current    = 0;
    line       = 1;
    parenDepth = 0;
    braceDepth = 0;
    tokens.clear();
}

std::vector<Token> Tokenizer::tokenize(const std::string& src) {
    reset(src);

    while (!isAtEnd()) {
        start = current;
        scanToken();
    }

    // 열린 괄호가 아직 안 닫혔다면 이 소스는 완결된 문장이 아니다.
    // (여는 괄호 없이 등장한 닫는 괄호는 depth를 0 밑으로 내리지 않으므로
    //  문법 오류 여부는 별개로 Assembler가 판단한다.)
    if (parenDepth > 0 || braceDepth > 0) {
        throw TokenizerIncompleteError("입력이 완결되지 않았습니다.");
    }

    tokens.push_back({ TokenType::END_OF_FILE, "", line });
    return tokens;
}

std::vector<Token> Tokenizer::tokenize(const std::string& src) {
    try {

    } catch (const TokenizerIncompleteError& e) {
        throw IncompleteInputError(line, e.what());
    } catch (const std::runtime_error& e) {
        throw AssemblyError(line, e.what());
    }
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
    switch (c) {
        case '(': addToken(TokenType::LEFT_PAREN);  ++parenDepth; break;
        case ')': addToken(TokenType::RIGHT_PAREN); if (parenDepth > 0) --parenDepth; break;
        case '{': addToken(TokenType::LEFT_BRACE);  ++braceDepth; break;
        case '}': addToken(TokenType::RIGHT_BRACE); if (braceDepth > 0) --braceDepth; break;
        case ';': addToken(TokenType::SEMICOLON);   break;
        case '+': addToken(TokenType::PLUS);        break;
        case '-': addToken(TokenType::MINUS);       break;
        case '*': addToken(TokenType::STAR);        break;
        case '/': addToken(TokenType::SLASH);       break;

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
        throw TokenizerIncompleteError("문자열이 종결되지 않았습니다.");

    advance(); // 닫는 '"' 소비
    // 따옴표를 제거한 실제 문자열 값을 origin에 저장
    std::string value = source.substr(start + 1, current - start - 2);
    tokens.push_back({ TokenType::STRING, value, line });
}

void Tokenizer::scanDefault(char c) {
    if (isDigit(c))      scanNumber();
    else if (isAlpha(c)) scanIdentifier();
    else throw std::runtime_error(std::string("알 수 없는 문자: '") + c + "'");
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
