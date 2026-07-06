#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Tokenizer.h"

using ::testing::ElementsAre;

class TokenizerFixture : public ::testing::Test {
protected:
    Tokenizer tokenizer;

    std::vector<TokenType> getTypes(const std::vector<Token>& tokens) {
        std::vector<TokenType> result;
        for (const auto& tok : tokens)
            result.push_back(tok.type);
        return result;
    }
};

TEST_F(TokenizerFixture, TokenTypes) {
    auto tokens = tokenizer.tokenize("print 1 + 2 * 3;");
    EXPECT_THAT(getTypes(tokens), ElementsAre(
        TokenType::PRINT,
        TokenType::NUMBER,
        TokenType::PLUS,
        TokenType::NUMBER,
        TokenType::STAR,
        TokenType::NUMBER,
        TokenType::SEMICOLON,
        TokenType::END_OF_FILE
    ));
}

TEST_F(TokenizerFixture, TokenOrigins) {
    auto tokens = tokenizer.tokenize("print 1 + 2 * 3;");
    EXPECT_EQ(tokens[0].origin, "print");
    EXPECT_EQ(tokens[1].origin, "1");
    EXPECT_EQ(tokens[2].origin, "+");
    EXPECT_EQ(tokens[3].origin, "2");
    EXPECT_EQ(tokens[4].origin, "*");
    EXPECT_EQ(tokens[5].origin, "3");
    EXPECT_EQ(tokens[6].origin, ";");
}

TEST_F(TokenizerFixture, TokenCount) {
    auto tokens = tokenizer.tokenize("print 1 + 2 * 3;");
    EXPECT_EQ(tokens.size(), 8u); // 7개 + END_OF_FILE
}