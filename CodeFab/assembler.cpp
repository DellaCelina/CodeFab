#include "assembler.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

// Binary operator precedence table, lowest to highest. parseExpression(level) consumes
// operators at operatorPriority[level], recursing into level + 1 for its operands; once
// level runs past the table, it falls through to unary/primary parsing.
// EQUAL (assignment) sits at the lowest level and is right-associative: its right
// operand recurses back into the SAME level instead of level + 1.
using OperatorPriority = std::vector<std::vector<TokenType>>;

const OperatorPriority kDefaultOperatorPriority = {
    { TokenType::EQUAL },
    { TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL },
    { TokenType::LESS, TokenType::LESS_EQUAL, TokenType::GREATER, TokenType::GREATER_EQUAL },
    { TokenType::PLUS, TokenType::MINUS },
    { TokenType::STAR, TokenType::SLASH },
};

// Grammar (lowest to highest precedence):
//   statement    -> printStmt | declareStmt | blockStmt | ifStmt | forStmt | exprStmt
//   printStmt    -> PRINT expression(0) SEMICOLON
//   declareStmt  -> VAR IDENTIFIER EQUAL expression(0) SEMICOLON
//   blockStmt    -> LEFT_BRACE statement* RIGHT_BRACE
//   ifStmt       -> IF LEFT_PAREN expression(0) RIGHT_PAREN statement (ELSE statement)?
//   forStmt      -> FOR LEFT_PAREN expression(0) SEMICOLON expression(0) SEMICOLON expression(0) RIGHT_PAREN statement
//   exprStmt     -> expression(0) SEMICOLON
//   expression(level) -> expression(level + 1) (operatorPriority[level] expression(level or level + 1))*
//   expression(operatorPriority.size()) -> unary
//   unary        -> (MINUS | BANG) unary | primary
//   primary      -> NUMBER | STRING | TRUE | FALSE | IDENTIFIER | LEFT_PAREN expression(0) RIGHT_PAREN
class Parser {
public:
    Parser(const std::vector<Token>& tokens, SyntaxTree& tree, const OperatorPriority& operatorPriority)
        : tokens(tokens), tree(tree), operatorPriority(operatorPriority) {}

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
        Expression* expr = parseExpression(0);
        Token semicolonToken = expectToken(TokenType::SEMICOLON, "Expect ';' after value.");
        return addNode<PrintStatement>(std::vector<Token>{ printToken, semicolonToken }, expr);
    }

    DeclareStatement* parseDeclareStatement() {
        Token varToken = advance();
        Token nameToken = expectToken(TokenType::IDENTIFIER, "Expect variable name.");
        IdentifierExpression* identifier = addNode<IdentifierExpression>(std::vector<Token>{ nameToken }, nameToken.origin);
        Token equalToken = expectToken(TokenType::EQUAL, "Expect '=' after variable name.");
        Expression* expr = parseExpression(0);
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
        Expression* expr = parseExpression(0);
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
        Expression* init = parseExpression(0);
        Token firstSemicolon = expectToken(TokenType::SEMICOLON, "Expect ';' after for-loop initializer.");
        Expression* compare = parseExpression(0);
        Token secondSemicolon = expectToken(TokenType::SEMICOLON, "Expect ';' after for-loop condition.");
        Expression* next = parseExpression(0);
        Token rightParen = expectToken(TokenType::RIGHT_PAREN, "Expect ')' after for-loop clauses.");
        Statement* loop = static_cast<Statement*>(parseStatement());

        return addNode<ForStatement>(
            std::vector<Token>{ forToken, leftParen, firstSemicolon, secondSemicolon, rightParen },
            init, compare, next, loop);
    }

    ExpressionStatement* parseExpressionStatement() {
        Expression* expr = parseExpression(0);
        Token semicolonToken = expectToken(TokenType::SEMICOLON, "Expect ';' after expression.");
        return addNode<ExpressionStatement>(std::vector<Token>{ semicolonToken }, expr);
    }

    // ---- Expressions ----

    // Consumes operators at operatorPriority[level], recursing into level + 1 for its
    // operands; past the end of the table, falls to parseUnary(). EQUAL is right-associative,
    // so its right operand recurses back into the same level instead of level + 1.
    Expression* parseExpression(size_t level) {
        if (level >= operatorPriority.size())
            return parseUnary();

        Expression* left = parseExpression(level + 1);
        while (!isAtEnd() && isOperatorAtLevel(level, peek().type)) {
            Token opToken = advance();
            Expression* right = opToken.type == TokenType::EQUAL
                ? parseExpression(level)
                : parseExpression(level + 1);
            left = makeBinaryExpression(opToken, left, right);
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
                Expression* expr = parseExpression(0);
                expectToken(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
                return expr;
            }
            default:
                throw std::invalid_argument("Expect expression.");
        }
    }

    // ---- Helpers ----

    bool isOperatorAtLevel(size_t level, TokenType type) const {
        const auto& operators = operatorPriority[level];
        return std::find(operators.begin(), operators.end(), type) != operators.end();
    }

    Expression* makeBinaryExpression(const Token& opToken, Expression* left, Expression* right) {
        switch (opToken.type) {
            case TokenType::EQUAL: {
                IdentifierExpression* identifier = dynamic_cast<IdentifierExpression*>(left);
                if (!identifier)
                    throw std::invalid_argument("Invalid assignment target.");
                return addNode<AssignExpression>(std::vector<Token>{ opToken }, identifier, right);
            }
            case TokenType::EQUAL_EQUAL: return addNode<EqualExpression>(std::vector<Token>{ opToken }, left, right);
            case TokenType::BANG_EQUAL: return addNode<NotEqualExpression>(std::vector<Token>{ opToken }, left, right);
            case TokenType::LESS: return addNode<LessExpression>(std::vector<Token>{ opToken }, left, right);
            case TokenType::LESS_EQUAL: return addNode<LessEqualExpression>(std::vector<Token>{ opToken }, left, right);
            case TokenType::GREATER: return addNode<GreaterExpression>(std::vector<Token>{ opToken }, left, right);
            case TokenType::GREATER_EQUAL: return addNode<GreaterEqualExpression>(std::vector<Token>{ opToken }, left, right);
            case TokenType::PLUS: return addNode<AddExpression>(std::vector<Token>{ opToken }, left, right);
            case TokenType::MINUS: return addNode<SubExpression>(std::vector<Token>{ opToken }, left, right);
            case TokenType::STAR: return addNode<MultExpression>(std::vector<Token>{ opToken }, left, right);
            default: return addNode<DivideExpression>(std::vector<Token>{ opToken }, left, right);
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
    const OperatorPriority& operatorPriority;
    size_t pos = 0;
};

}  // namespace

std::unique_ptr<SyntaxTree> Assembler::assemble(const std::vector<Token> tokens) {
    auto tree = std::make_unique<SyntaxTree>();
    Parser parser(tokens, *tree, kDefaultOperatorPriority);
    tree->setRoot(parser.parseStatement());
    return tree;
}
