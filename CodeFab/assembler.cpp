#include "assembler.h"

#include <stdexcept>

namespace {

// Grammar (STAR binds tighter than PLUS):
//   statement  -> PRINT expression SEMICOLON
//   expression -> term (PLUS term)*
//   term       -> factor (STAR factor)*
//   factor     -> NUMBER
class Parser {
public:
    Parser(const std::vector<Token>& tokens, SyntaxTree& tree) : tokens(tokens), tree(tree) {}

    PrintStatement* parseStatement() {
        Token printToken = tokens[pos];
        expect(TokenType::PRINT);
        Expression* expr = parseExpression();
        Token semicolonToken = tokens[pos];
        expect(TokenType::SEMICOLON);

        auto statement = std::make_unique<PrintStatement>(std::vector<Token>{ printToken, semicolonToken }, expr);
        PrintStatement* raw = statement.get();
        tree.add(std::move(statement));
        return raw;
    }

private:
    Expression* parseExpression() {
        Expression* left = parseTerm();
        while (pos < tokens.size() && tokens[pos].type == TokenType::PLUS) {
            Token opToken = tokens[pos];
            pos++;
            Expression* right = parseTerm();

            auto add = std::make_unique<AddExpression>(std::vector<Token>{ opToken }, left, right);
            left = add.get();
            tree.add(std::move(add));
        }
        return left;
    }

    Expression* parseTerm() {
        Expression* left = parseFactor();
        while (pos < tokens.size() && tokens[pos].type == TokenType::STAR) {
            Token opToken = tokens[pos];
            pos++;
            Expression* right = parseFactor();

            auto mult = std::make_unique<MultExpression>(std::vector<Token>{ opToken }, left, right);
            left = mult.get();
            tree.add(std::move(mult));
        }
        return left;
    }

    Expression* parseFactor() {
        const Token& token = tokens[pos];
        expect(TokenType::NUMBER);

        auto number = std::make_unique<NumberExpression>(std::vector<Token>{ token }, std::stod(token.orign));
        Expression* raw = number.get();
        tree.add(std::move(number));
        return raw;
    }

    void expect(TokenType type) {
        if (pos >= tokens.size() || tokens[pos].type != type)
            throw std::runtime_error("unexpected token on line");
        pos++;
    }

    const std::vector<Token>& tokens;
    SyntaxTree& tree;
    size_t pos = 0;
};

}  // namespace

std::unique_ptr<SyntaxTree> Assembler::assemble(const std::vector<Token> tokens) {
    auto tree = std::make_unique<SyntaxTree>();
    Parser parser(tokens, *tree);
    tree->setRoot(parser.parseStatement());
    return tree;
}
