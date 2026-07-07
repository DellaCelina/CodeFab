#include "Assembler.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using Tokens = std::vector<Token>;

namespace {

// Binary operator precedence table, lowest to highest. parseExpression(level) consumes
// operators at operatorPriority[level], recursing into level + 1 for its operands; once
// level runs past the table, it falls through to unary/primary parsing.
// EQUAL (assignment) sits at the lowest level and is right-associative: its right
// operand recurses back into the SAME level instead of level + 1.
using OperatorsPriority = std::vector<std::vector<TokenType>>;

const OperatorsPriority kDefaultOperatorPriority = {
    { TokenType::EQUAL },
    { TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL },
    { TokenType::LESS, TokenType::LESS_EQUAL, TokenType::GREATER, TokenType::GREATER_EQUAL },
    { TokenType::PLUS, TokenType::MINUS },
    { TokenType::STAR, TokenType::SLASH },
};

// Prefix operators parseUnary() recognizes, e.g. -x, !x.
using UnaryOperators = std::vector<TokenType>;

const UnaryOperators kDefaultUnaryOperator = { TokenType::MINUS, TokenType::BANG };

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
    Parser(const Tokens& tokens, SyntaxTree& tree, const OperatorsPriority& operatorPriority,
        const UnaryOperators& unaryOperator)
        : tokens(tokens), tree(tree), operatorPriority(operatorPriority), unaryOperator(unaryOperator) {}

    SyntaxNode* parseStatement() {
        if (auto token = currentToken()) {
            switch (token->type) {
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
        Token printToken = popToken();
        Expression* expr = parseExpression(0);
        Token semicolonToken = popExpectedToken(TokenType::SEMICOLON, "Expect ';' after value.");
        return addNode<PrintStatement>(Tokens{ printToken, semicolonToken }, expr);
    }

    DeclareStatement* parseDeclareStatement() {
        Token varToken = popToken();
        Token nameToken = popExpectedToken(TokenType::IDENTIFIER, "Expect variable name.");
        IdentifierExpression* identifier = addNode<IdentifierExpression>(Tokens{ nameToken }, nameToken.origin);
        Token equalToken = popExpectedToken(TokenType::EQUAL, "Expect '=' after variable name.");
        Expression* expr = parseExpression(0);
        Token semicolonToken = popExpectedToken(TokenType::SEMICOLON, "Expect ';' after value.");
        return addNode<DeclareStatement>(Tokens{ varToken, equalToken, semicolonToken }, identifier, expr);
    }

    BlockStatement* parseBlockStatement() {
        Token leftBrace = popToken();
        std::vector<Statement*> statements;
        while (auto token = currentToken()) {
            if (token->type == TokenType::RIGHT_BRACE)
                break;
            statements.push_back(static_cast<Statement*>(parseStatement()));
        }
        Token rightBrace = popExpectedToken(TokenType::RIGHT_BRACE, "Expect '}' after block.");
        return addNode<BlockStatement>(Tokens{ leftBrace, rightBrace }, statements);
    }

    IfStatement* parseIfStatement() {
        Token ifToken = popToken();
        Token leftParen = popExpectedToken(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
        Expression* expr = parseExpression(0);
        Token rightParen = popExpectedToken(TokenType::RIGHT_PAREN, "Expect ')' after if condition.");
        Statement* thenBranch = static_cast<Statement*>(parseStatement());

        Tokens ownTokens{ ifToken, leftParen, rightParen };
        Statement* elseBranch = nullptr;
        if (auto token = currentToken(); token && token->type == TokenType::ELSE) {
            ownTokens.push_back(popToken());
            elseBranch = static_cast<Statement*>(parseStatement());
        }
        return addNode<IfStatement>(ownTokens, expr, thenBranch, elseBranch);
    }

    ForStatement* parseForStatement() {
        Token forToken = popToken();
        Token leftParen = popExpectedToken(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");
        Expression* init = parseExpression(0);
        Token firstSemicolon = popExpectedToken(TokenType::SEMICOLON, "Expect ';' after for-loop initializer.");
        Expression* compare = parseExpression(0);
        Token secondSemicolon = popExpectedToken(TokenType::SEMICOLON, "Expect ';' after for-loop condition.");
        Expression* next = parseExpression(0);
        Token rightParen = popExpectedToken(TokenType::RIGHT_PAREN, "Expect ')' after for-loop clauses.");
        Statement* loop = static_cast<Statement*>(parseStatement());

        return addNode<ForStatement>(
            Tokens{ forToken, leftParen, firstSemicolon, secondSemicolon, rightParen },
            init, compare, next, loop);
    }

    ExpressionStatement* parseExpressionStatement() {
        Expression* expr = parseExpression(0);
        Token semicolonToken = popExpectedToken(TokenType::SEMICOLON, "Expect ';' after expression.");
        return addNode<ExpressionStatement>(Tokens{ semicolonToken }, expr);
    }

    // ---- Expressions ----

    // Consumes operators at operatorPriority[level], recursing into level + 1 for its
    // operands; past the end of the table, falls to parseUnary(). EQUAL is right-associative,
    // so its right operand recurses back into the same level instead of level + 1.
    Expression* parseExpression(size_t level) {
        if (level >= operatorPriority.size())
            return parseUnary();

        Expression* left = parseExpression(level + 1);
        while (auto token = currentToken()) {
            if (!isOperatorAtLevel(level, token->type))
                break;
            Token opToken = popToken();

            auto nextLevel = opToken.type == TokenType::EQUAL ? level : level + 1;
            Expression* right = parseExpression(nextLevel);

            left = makeBinaryExpression(opToken, left, right);
        }
        return left;
    }

    Expression* parseUnary() {
        if (auto token = currentToken(); token && isUnaryOperator(token->type)) {
            Token opToken = popToken();
            Expression* operand = parseUnary();
            return makeUnaryExpression(opToken, operand);
        }
        return parsePrimary();
    }

    Expression* parsePrimary() {
        auto token = currentToken();
        if (!token)
            throw AssemblerError("No more token for expression.");

        switch (token->type) {
            case TokenType::NUMBER:
                popToken();
                return addNode<NumberExpression>(Tokens{ *token }, std::stod(token->origin));
            case TokenType::STRING:
                popToken();
                return addNode<StringExpression>(Tokens{ *token }, token->origin);
            case TokenType::TRUE:
                popToken();
                return addNode<BooleanExpression>(Tokens{ *token }, true);
            case TokenType::FALSE:
                popToken();
                return addNode<BooleanExpression>(Tokens{ *token }, false);
            case TokenType::IDENTIFIER:
                popToken();
                return addNode<IdentifierExpression>(Tokens{ *token }, token->origin);
            case TokenType::LEFT_PAREN: {
                popToken();
                Expression* expr = parseExpression(0);
                popExpectedToken(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
                return expr;
            }
            default:
                throw makeParseError("Expect expression.", *token);
        }
    }

    // ---- Helpers ----

    bool isOperatorAtLevel(size_t level, TokenType type) const {
        const auto& operators = operatorPriority[level];
        return std::find(operators.begin(), operators.end(), type) != operators.end();
    }

    bool isUnaryOperator(TokenType type) const {
        return std::find(unaryOperator.begin(), unaryOperator.end(), type) != unaryOperator.end();
    }

    // Attaches the offending token's origin/line to a parse error message, when the
    // token that caused the error is known.
    static AssemblerError makeParseError(const std::string& message, const Token& token) {
        return AssemblerError("{} (near '{}' at line {})", message, token.origin, token.line);
    }

    Expression* makeBinaryExpression(const Token& opToken, Expression* left, Expression* right) {
        switch (opToken.type) {
            case TokenType::EQUAL: {
                IdentifierExpression* identifier = dynamic_cast<IdentifierExpression*>(left);
                if (!identifier)
                    throw makeParseError("Invalid assignment target.", opToken);
                return addNode<AssignExpression>(Tokens{ opToken }, identifier, right);
            }
            case TokenType::EQUAL_EQUAL: return addNode<EqualExpression>(Tokens{ opToken }, left, right);
            case TokenType::BANG_EQUAL: return addNode<NotEqualExpression>(Tokens{ opToken }, left, right);
            case TokenType::LESS: return addNode<LessExpression>(Tokens{ opToken }, left, right);
            case TokenType::LESS_EQUAL: return addNode<LessEqualExpression>(Tokens{ opToken }, left, right);
            case TokenType::GREATER: return addNode<GreaterExpression>(Tokens{ opToken }, left, right);
            case TokenType::GREATER_EQUAL: return addNode<GreaterEqualExpression>(Tokens{ opToken }, left, right);
            case TokenType::PLUS: return addNode<AddExpression>(Tokens{ opToken }, left, right);
            case TokenType::MINUS: return addNode<SubExpression>(Tokens{ opToken }, left, right);
            case TokenType::STAR: return addNode<MultExpression>(Tokens{ opToken }, left, right);
            default: return addNode<DivideExpression>(Tokens{ opToken }, left, right);
        }
    }

    Expression* makeUnaryExpression(const Token& opToken, Expression* operand) {
        switch (opToken.type) {
            case TokenType::MINUS: return addNode<NegativeExpression>(Tokens{ opToken }, operand);
            default: return addNode<NotExpression>(Tokens{ opToken }, operand);
        }
    }

    std::optional<Token> currentToken() const {
        if (pos >= tokens.size())
            return std::nullopt;
        return tokens[pos];
    }

    Token popToken() {
        return tokens[pos++];
    }

    Token popExpectedToken(TokenType type, const std::string& message) {
        auto token = currentToken();
        if (!token)
            throw AssemblerError(message);
        if (token->type != type)
            throw makeParseError(message, *token);
        return popToken();
    }

    template <typename NodeType, typename... Args>
    NodeType* addNode(const Tokens& nodeTokens, Args&&... args) {
        auto node = std::make_unique<NodeType>(nodeTokens, std::forward<Args>(args)...);
        NodeType* raw = node.get();
        tree.add(std::move(node));
        return raw;
    }

    const Tokens& tokens;
    SyntaxTree& tree;
    const OperatorsPriority operatorPriority;
    const UnaryOperators unaryOperator;
    size_t pos = 0;
};

}  // namespace

SyntaxTree Assembler::assemble(const Tokens& tokens) {
    SyntaxTree tree;
    Parser parser(tokens, tree, kDefaultOperatorPriority, kDefaultUnaryOperator);
    tree.setRoot(parser.parseStatement());
    return std::move(tree);
}
