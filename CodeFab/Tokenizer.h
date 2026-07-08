#pragma once
#include "Token.h"
#include "TokenizeInterface.h"
#include <stdexcept>
#include <vector>
#include <string>

class Tokenizer : public TokenizeInterface {
public:
    // TokenizeInterface 구현: scanTokens() 중 발견한 문법 오류를 AssemblyError로
    // 변환해서 던진다 (줄 번호를 메시지에 담는다). 괄호/문자열이 아직 안 닫힌
    // 경우에는 scanTokens()가 직접 IncompleteInputError를 던진다.
    std::vector<Token> tokenize(const std::string& source) override;

private:
    std::string        source;
    int                start      = 0;
    int                current    = 0;
    int                line       = 1;
    int                parenDepth = 0;
    int                braceDepth = 0;
    std::vector<Token> tokens;

    std::vector<Token> scanTokens(const std::string& source);

    bool isAtEnd();
    void scanToken();
    void addToken(TokenType type);
    char advance();
    bool match(char expected);
    char peek();
    char peekNext();
    void scanString();
    void scanNumber();
    void scanIdentifier();

    void reset(const std::string& src);
    bool isDigit(char c);
    bool isAlpha(char c);
    bool isAlphaNumeric(char c);
    void scanDefault(char c);
};
