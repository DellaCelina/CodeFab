#pragma once
#include "Token.h"
#include "TokenizeInterface.h"
#include <stdexcept>
#include <vector>
#include <string>

// scanTokens() 도중 소스가 아직 끝나지 않은 괄호/문자열을 만나면 던진다.
// (실제 문법 오류가 아니라 "입력이 더 필요하다"는 신호)
class TokenizerIncompleteError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Tokenizer : public TokenizeInterface {
public:
    // TokenizeInterface 구현: scanTokens()를 감싸서 예외를
    // IncompleteInputError / AssemblyError로 변환한다.
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
