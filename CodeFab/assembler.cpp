#include "assembler.h"

#include <stdexcept>
#include <utility>

namespace {

// Grammar (lowest to highest precedence):
//   statement    -> printStmt | declareStmt | blockStmt | ifStmt | forStmt | exprStmt
//   printStmt    -> PRINT assignment SEMICOLON
//   declareStmt  -> VAR IDENTIFIER EQUAL assignment SEMICOLON
//   blockStmt    -> LEFT_BRACE statement* RIGHT_BRACE
//   ifStmt       -> IF LEFT_PAREN assignment RIGHT_PAREN statement (ELSE statement)?
//   forStmt      -> FOR LEFT_PAREN assignment SEMICOLON assignment SEMICOLON assignment RIGHT_PAREN statement
//   exprStmt     -> assignment SEMICOLON
//   assignment   -> IDENTIFIER EQUAL assignment | equality
//   equality     -> comparison ((EQUAL_EQUAL | BANG_EQUAL) comparison)*
//   comparison   -> addition ((LESS | LESS_EQUAL | GREATER | GREATER_EQUAL) addition)*
//   addition     -> multiplication ((PLUS | MINUS) multiplication)*
//   multiplication -> unary ((STAR | SLASH) unary)*
//   unary        -> (MINUS | BANG) unary | primary
//   primary      -> NUMBER | STRING | TRUE | FALSE | IDENTIFIER | LEFT_PAREN assignment RIGHT_PAREN
class Parser {
public:
    Parser(const std::vector<Token>& tokens, SyntaxTree& tree) : tokens(tokens), tree(tree) {}

    SyntaxNode* parseStatement() {
        if (!isAtEnd()) {
            switch (peek().type) {
                case TokenType::PRINT: return parsePrintStatement();
                case TokenType::VAR: return parseDeclareStatement();
                case TokenType::LEFT_BRACE: return parseBlockStatement();
                case TokenType::IF: return parseIfStatement();
                case TokenType::FOR: return parseForStatement();
                default: break;
            }
        }
        return parseExpressionStatement();
    }

private:
    // ---- Statements ----

    PrintStatement* parsePrintStatement() {
        Token printToken = advance();
        Expression* expr = parseAssignment(false);
        Token semicolonToken = expectToken(TokenType::SEMICOLON, "Expect ';' after value.");
        return addNode<PrintStatement>(std::vector<Token>{ printToken, semicolonToken }, expr);
    }

    DeclareStatement* parseDeclareStatement() {
        Token varToken = advance();
        Token nameToken = expectToken(TokenType::IDENTIFIER, "Expect variable name.");
        IdentifierExpression* identifier = addNode<IdentifierExpression>(std::vector<Token>{ nameToken }, nameToken.origin);
        Token equalToken = expectToken(TokenType::EQUAL, "Expect '=' after variable name.");
        Expression* expr = parseAssignment(false);
        Token semicolonToken = expectToken(TokenType::SEMICOLON, "Expect ';' after value.");
        return addNode<DeclareStatement>(std::vector<Token>{ varToken, equalToken, semicolonToken }, identifier, expr);
    }

    BlockStatement* parseBlockStatement() {
        Token leftBrace = advance();
        std::vector<Statement*> statements;
        while (!isAtEnd() && peek().type != TokenType::RIGHT_BRACE)
            statements.push_back(static_cast<Statement*>(parseStatement()));
        Token rightBrace = expectToken(TokenType::RIGHT_BRACE, "Expect '}' after block.");
        return addNode<BlockStatement>(std::vector<Token>{ leftBrace, rightBrace }, statements);
    }

    IfStatement* parseIfStatement() {
        Token ifToken = advance();
        Token leftParen = expectToken(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
        Expression* expr = parseAssignment(false);
        Token rightParen = expectToken(TokenType::RIGHT_PAREN, "Expect ')' after if condition.");
        Statement* thenBranch = static_cast<Statement*>(parseStatement());

        std::vector<Token> ownTokens{ ifToken, leftParen, rightParen };
        Statement* elseBranch = nullptr;
        if (!isAtEnd() && peek().type == TokenType::ELSE) {
            ownTokens.push_back(advance());
            elseBranch = static_cast<Statement*>(parseStatement());
        }
        return addNode<IfStatement>(ownTokens, expr, thenBranch, elseBranch);
    }

    ForStatement* parseForStatement() {
        Token forToken = advance();
        Token leftParen = expectToken(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");
        Expression* init = parseAssignment(false);
        Token firstSemicolon = expectToken(TokenType::SEMICOLON, "Expect ';' after for-loop initializer.");
        Expression* compare = parseAssignment(false);
        Token secondSemicolon = expectToken(TokenType::SEMICOLON, "Expect ';' after for-loop condition.");
        Expression* next = parseAssignment(false);
        Token rightParen = expectToken(TokenType::RIGHT_PAREN, "Expect ')' after for-loop clauses.");
        Statement* loop = static_cast<Statement*>(parseStatement());

        return addNode<ForStatement>(
            std::vector<Token>{ forToken, leftParen, firstSemicolon, secondSemicolon, rightParen },
            init, compare, next, loop);
    }

    Expression* parseExpressionStatement() {
        return parseAssignment(true);
    }

    // ---- Expressions ----

    Expression* parseAssignment(bool asStatement) {
        Expression* expr = parseEquality();

        if (!isAtEnd() && peek().type == TokenType::EQUAL) {
            IdentifierExpression* identifier = dynamic_cast<IdentifierExpression*>(expr);
            if (!identifier)
                throw std::invalid_argument("Invalid assignment target.");

            Token equalToken = advance();
            Expression* value = parseAssignment(false);

            std::vector<Token> ownTokens{ equalToken };
            if (asStatement)
                ownTokens.push_back(expectToken(TokenType::SEMICOLON, "Expect ';' after value."));

            return addNode<AssignExpression>(ownTokens, identifier, value);
        }

        return expr;
    }

    Expression* parseEquality() {
        Expression* left = parseComparison();
        while (!isAtEnd() && (peek().type == TokenType::EQUAL_EQUAL || peek().type == TokenType::BANG_EQUAL)) {
            Token opToken = advance();
            Expression* right = parseComparison();
            left = opToken.type == TokenType::EQUAL_EQUAL
                ? static_cast<Expression*>(addNode<EqualExpression>(std::vector<Token>{ opToken }, left, right))
                : static_cast<Expression*>(addNode<NotEqualExpression>(std::vector<Token>{ opToken }, left, right));
        }
        return left;
    }

    Expression* parseComparison() {
        Expression* left = parseAddition();
        while (!isAtEnd() && isComparisonOperator(peek().type)) {
            Token opToken = advance();
            Expression* right = parseAddition();
            left = makeComparisonExpression(opToken, left, right);
        }
        return left;
    }

    Expression* parseAddition() {
        Expression* left = parseMultiplication();
        while (!isAtEnd() && (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS)) {
            Token opToken = advance();
            Expression* right = parseMultiplication();
            left = opToken.type == TokenType::PLUS
                ? static_cast<Expression*>(addNode<AddExpression>(std::vector<Token>{ opToken }, left, right))
                : static_cast<Expression*>(addNode<SubExpression>(std::vector<Token>{ opToken }, left, right));
        }
        return left;
    }

    Expression* parseMultiplication() {
        Expression* left = parseUnary();
        while (!isAtEnd() && (peek().type == TokenType::STAR || peek().type == TokenType::SLASH)) {
            Token opToken = advance();
            Expression* right = parseUnary();
            left = opToken.type == TokenType::STAR
                ? static_cast<Expression*>(addNode<MultExpression>(std::vector<Token>{ opToken }, left, right))
                : static_cast<Expression*>(addNode<DivideExpression>(std::vector<Token>{ opToken }, left, right));
        }
        return left;
    }

    Expression* parseUnary() {
        if (!isAtEnd() && (peek().type == TokenType::MINUS || peek().type == TokenType::BANG)) {
            Token opToken = advance();
            Expression* operand = parseUnary();
            return opToken.type == TokenType::MINUS
                ? static_cast<Expression*>(addNode<NegativeExpression>(std::vector<Token>{ opToken }, operand))
                : static_cast<Expression*>(addNode<NotExpression>(std::vector<Token>{ opToken }, operand));
        }
        return parsePrimary();
    }

    Expression* parsePrimary() {
        if (isAtEnd())
            throw std::invalid_argument("Expect expression.");

        Token token = peek();
        switch (token.type) {
            case TokenType::NUMBER:
                advance();
                return addNode<NumberExpression>(std::vector<Token>{ token }, std::stod(token.origin));
            case TokenType::STRING:
                advance();
                return addNode<StringExpression>(std::vector<Token>{ token }, token.origin);
            case TokenType::TRUE:
                advance();
                return addNode<BooleanExpression>(std::vector<Token>{ token }, true);
            case TokenType::FALSE:
                advance();
                return addNode<BooleanExpression>(std::vector<Token>{ token }, false);
            case TokenType::IDENTIFIER:
                advance();
                return addNode<IdentifierExpression>(std::vector<Token>{ token }, token.origin);
            case TokenType::LEFT_PAREN: {
                advance();
                Expression* expr = parseAssignment(false);
                expectToken(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
                return expr;
            }
            default:
                throw std::invalid_argument("Expect expression.");
        }
    }

    // ---- Helpers ----

    static bool isComparisonOperator(TokenType type) {
        return type == TokenType::LESS || type == TokenType::LESS_EQUAL
            || type == TokenType::GREATER || type == TokenType::GREATER_EQUAL;
    }

    Expression* makeComparisonExpression(const Token& opToken, Expression* left, Expression* right) {
        switch (opToken.type) {
            case TokenType::LESS: return addNode<LessExpression>(std::vector<Token>{ opToken }, left, right);
            case TokenType::LESS_EQUAL: return addNode<LessEqualExpression>(std::vector<Token>{ opToken }, left, right);
            case TokenType::GREATER: return addNode<GreaterExpression>(std::vector<Token>{ opToken }, left, right);
            default: return addNode<GreaterEqualExpression>(std::vector<Token>{ opToken }, left, right);
        }
    }

    bool isAtEnd() const {
        return pos >= tokens.size();
    }

    const Token& peek() const {
        return tokens[pos];
    }

    Token advance() {
        return tokens[pos++];
    }

    Token expectToken(TokenType type, const std::string& message) {
        if (isAtEnd() || tokens[pos].type != type)
            throw std::invalid_argument(message);
        return tokens[pos++];
    }

    template <typename NodeType, typename... Args>
    NodeType* addNode(const std::vector<Token>& nodeTokens, Args&&... args) {
        auto node = std::make_unique<NodeType>(nodeTokens, std::forward<Args>(args)...);
        NodeType* raw = node.get();
        tree.add(std::move(node));
        return raw;
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
