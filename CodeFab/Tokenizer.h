#pragma once
#include "Token.h"
#include <vector>
#include <string>

class Tokenizer {
public:
    std::vector<Token> tokenize(const std::string& source);

private:
    std::string        source;
    int                start   = 0;
    int                current = 0;
    int                line    = 1;
    std::vector<Token> tokens;

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
};
