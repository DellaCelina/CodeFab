#pragma once
#include "Token.h"
#include "TokenizeInterface.h"
#include <vector>
#include <string>

class Tokenizer : public TokenizeInterface {
public:
    // TokenizeInterface 구현: 인식 불가능한 문자 또는 종결되지 않은 문자열을
    // 만나면 AssemblyError를 던진다 (줄 번호를 메시지에 담는다).
    std::vector<Token> tokenize(const std::string& source) override;

private:
    std::string        source;
    int                start   = 0;
    int                current = 0;
    int                line    = 1;
    std::vector<Token> tokens;

    void reset(const std::string& src);
    bool isAtEnd();
    char advance();
    bool match(char expected);
    char peek();
    char peekNext();
    void addToken(TokenType type);
    void addToken(TokenType type, std::string value);
    void scanToken();
    void scanString();
    void scanNumber();
    void scanIdentifier();
    void scanDefault(char c);

    static bool isDigit(char c);
    static bool isAlpha(char c);
    static bool isAlphaNumeric(char c);
};
