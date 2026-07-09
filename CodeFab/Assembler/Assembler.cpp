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
// INSTANCEOF is handled specially inside parseExpression() (its right-hand side is a
// class-name Token, not a recursively-parsed Expression), but shares the comparison
// level's precedence slot.
using OperatorsPriority = std::vector<std::vector<TokenType>>;

const OperatorsPriority kDefaultOperatorPriority = {
    { TokenType::EQUAL },
    { TokenType::OR },
    { TokenType::AND },
    { TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL },
    { TokenType::LESS, TokenType::LESS_EQUAL, TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::INSTANCEOF },
    { TokenType::PLUS, TokenType::MINUS },
    { TokenType::STAR, TokenType::SLASH, TokenType::PERCENT },
};

// Prefix operators parseUnary() recognizes, e.g. -x, !x.
using UnaryOperators = std::vector<TokenType>;

const UnaryOperators kDefaultUnaryOperator = { TokenType::MINUS, TokenType::BANG };

// Grammar (lowest to highest precedence):
//   statement      -> printStmt | declareStmt | blockStmt | ifStmt | forStmt
//                    | funcDeclStmt | classDeclStmt | returnStmt | importStmt | exprStmt
//   printStmt      -> PRINT expression(0) SEMICOLON
//   declareStmt    -> VAR IDENTIFIER EQUAL expression(0) SEMICOLON
//   blockStmt      -> LEFT_BRACE statement* RIGHT_BRACE
//   ifStmt         -> IF LEFT_PAREN expression(0) RIGHT_PAREN statement (ELSE statement)?
//   forStmt        -> FOR LEFT_PAREN forInit SEMICOLON expression(0) SEMICOLON expression(0) RIGHT_PAREN statement
//   forInit        -> declareStmt | expression(0) SEMICOLON
//   funcDeclStmt   -> FUNC IDENTIFIER LEFT_PAREN params? RIGHT_PAREN blockStmt
//   params         -> IDENTIFIER (COMMA IDENTIFIER)*
//   classDeclStmt  -> CLASS IDENTIFIER LEFT_BRACE methodDeclStmt* RIGHT_BRACE
//   methodDeclStmt -> IDENTIFIER LEFT_PAREN params? RIGHT_PAREN blockStmt   // Func 없음
//   returnStmt     -> RETURN expression(0)? SEMICOLON
//   importStmt     -> IMPORT STRING ALIAS IDENTIFIER SEMICOLON
//   exprStmt       -> expression(0) SEMICOLON
//   expression(level) -> expression(level + 1) (operatorPriority[level] expression(level or level + 1))*
//   expression(operatorPriority.size()) -> unary
//   unary        -> (MINUS | BANG) unary | call
//   call         -> primary ( "(" arguments? ")" | "." IDENTIFIER | "[" expression(0) "]" )*
//   arguments    -> expression(0) (COMMA expression(0))*
//   primary      -> NUMBER | STRING | TRUE | FALSE | THIS | IDENTIFIER
//                 | ARRAY "(" expression(0) ")"
//                 | LEFT_PAREN expression(0) RIGHT_PAREN
class Parser {
public:
    Parser(const Tokens& tokens, SyntaxTree& tree, const OperatorsPriority& operatorPriority,
        const UnaryOperators& unaryOperator, SourceReaderInterface& sourceReader,
        std::vector<std::string>& importStack)
        : tokens(tokens), tree(tree), operatorPriority(operatorPriority), unaryOperator(unaryOperator),
          sourceReader(sourceReader), importStack(importStack) {}

    SyntaxNode* parseStatement() {
        if (auto token = currentToken()) {
            switch (token->type) {
                case TokenType::PRINT: return parsePrintStatement();
                case TokenType::VAR: return parseDeclareStatement();
                case TokenType::LEFT_BRACE: return parseBlockStatement();
                case TokenType::IF: return parseIfStatement();
                case TokenType::FOR: return parseForStatement();
                case TokenType::FUNC: return parseFunction();
                case TokenType::CLASS: return parseClass();
                case TokenType::RETURN: return parseReturnStatement();
                case TokenType::IMPORT: return parseImport();
                default: break;
            }
        }
        return parseExpressionStatement();
    }

    // 소스 전체(주로 import 대상 파일)를 끝까지 최상위 statement로 나눠 파싱한다.
    // Assembler::assemble()이 만드는 단일 root와 달리, import는 여러 최상위
    // 선언을 한 번에 필요로 하므로 이 메서드를 별도로 둔다.
    std::vector<Statement*> parseProgram() {
        std::vector<Statement*> statements;
        while (currentToken()) {
            statements.push_back(static_cast<Statement*>(parseStatement()));
        }
        return statements;
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
        Statement* init = parseForInitializer();
        Expression* compare = parseExpression(0);
        Token secondSemicolon = popExpectedToken(TokenType::SEMICOLON, "Expect ';' after for-loop condition.");
        Expression* next = parseExpression(0);
        Token rightParen = popExpectedToken(TokenType::RIGHT_PAREN, "Expect ')' after for-loop clauses.");
        Statement* loop = static_cast<Statement*>(parseStatement());

        return addNode<ForStatement>(
            Tokens{ forToken, leftParen, secondSemicolon, rightParen },
            init, compare, next, loop);
    }

    // 초기화절은 `var j = 0` 같은 선언(declareStmt, 세미콜론까지 직접 소비)이거나
    // `j = 0` 같은 일반 expression(세미콜론은 여기서 소비해 ExpressionStatement로 감싼다)이다.
    Statement* parseForInitializer() {
        if (auto token = currentToken(); token && token->type == TokenType::VAR) {
            return parseDeclareStatement();
        }
        Expression* initExpr = parseExpression(0);
        Token semicolonToken = popExpectedToken(TokenType::SEMICOLON, "Expect ';' after for-loop initializer.");
        return addNode<ExpressionStatement>(Tokens{ semicolonToken }, initExpr);
    }

    ExpressionStatement* parseExpressionStatement() {
        Expression* expr = parseExpression(0);
        Token semicolonToken = popExpectedToken(TokenType::SEMICOLON, "Expect ';' after expression.");
        return addNode<ExpressionStatement>(Tokens{ semicolonToken }, expr);
    }

    // 이름/파라미터 목록/바디만 파싱하고, 만들 노드 타입은 호출부(parseFunction/
    // parseMethod)가 정한다 - FUNC 토큰을 소비하느냐만 다르고 나머지는 완전히
    // 같기 때문에 이 공용 헬퍼로 중복을 없앤다.
    struct ParsedFunctionParts {
        Token name;
        std::vector<Token> params;
        std::vector<Statement*> body;
        Token rightParen;
        Token leftBrace;
        Token rightBrace;
    };

    ParsedFunctionParts parseFunctionParts() {
        Token name = popExpectedToken(TokenType::IDENTIFIER, "Expect name.");
        popExpectedToken(TokenType::LEFT_PAREN, "Expect '(' after name.");
        std::vector<Token> params;
        if (auto t = currentToken(); t && t->type != TokenType::RIGHT_PAREN) {
            params.push_back(popExpectedToken(TokenType::IDENTIFIER, "Expect parameter name."));
            for (auto comma = currentToken(); comma && comma->type == TokenType::COMMA; comma = currentToken()) {
                popToken();
                params.push_back(popExpectedToken(TokenType::IDENTIFIER, "Expect parameter name."));
            }
        }
        Token rightParen = popExpectedToken(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
        Token leftBrace = popExpectedToken(TokenType::LEFT_BRACE, "Expect '{' before body.");
        std::vector<Statement*> body;
        for (auto t = currentToken(); t && t->type != TokenType::RIGHT_BRACE; t = currentToken()) {
            body.push_back(static_cast<Statement*>(parseStatement()));
        }
        Token rightBrace = popExpectedToken(TokenType::RIGHT_BRACE, "Expect '}' after body.");
        return { name, params, body, rightParen, leftBrace, rightBrace };
    }

    // 최상위 함수: FUNC 토큰을 먼저 소비한다.
    FunctionDeclareStatement* parseFunction() {
        Token funcToken = popToken();
        auto parts = parseFunctionParts();
        return addNode<FunctionDeclareStatement>(
            Tokens{ funcToken, parts.rightParen, parts.leftBrace, parts.rightBrace },
            parts.name, parts.params, parts.body);
    }

    // 클래스 메서드: FUNC 토큰이 없다 - 클래스 바디 안에서만(parseClass) 호출된다.
    MethodDeclareStatement* parseMethod() {
        auto parts = parseFunctionParts();
        return addNode<MethodDeclareStatement>(
            Tokens{ parts.rightParen, parts.leftBrace, parts.rightBrace },
            parts.name, parts.params, parts.body);
    }

    ClassDeclareStatement* parseClass() {
        Token classToken = popToken();
        Token name = popExpectedToken(TokenType::IDENTIFIER, "Expect class name.");

        IdentifierExpression* superclass = nullptr;
        if (auto t = currentToken(); t && t->type == TokenType::COLON) {
            popToken();
            Token superName = popExpectedToken(TokenType::IDENTIFIER, "Expect superclass name after ':'.");
            superclass = addNode<IdentifierExpression>(Tokens{ superName }, superName.origin);
        }

        Token leftBrace = popExpectedToken(TokenType::LEFT_BRACE, "Expect '{' before class body.");
        std::vector<MethodDeclareStatement*> methods;
        // 클래스 바디는 일반 statement가 아니라 메서드 선언만 허용되는 별도
        // 문법 위치이므로 parseStatement()가 아니라 곧바로 parseMethod()만
        // 호출한다 - Implement.md §2 "구현 순서 제안" 4번 참고.
        for (auto t = currentToken(); t && t->type != TokenType::RIGHT_BRACE; t = currentToken()) {
            methods.push_back(parseMethod());
        }
        Token rightBrace = popExpectedToken(TokenType::RIGHT_BRACE, "Expect '}' after class body.");
        return addNode<ClassDeclareStatement>(Tokens{ classToken, leftBrace, rightBrace }, name, methods, superclass);
    }

    // return이 함수/메서드 밖에서 쓰였는지는 의미 검사(Checker)의 몫이므로,
    // Assembler는 문맥을 가리지 않고 파싱만 한다.
    ReturnStatement* parseReturnStatement() {
        Token returnToken = popToken();
        if (auto t = currentToken(); t && t->type == TokenType::SEMICOLON) {
            Token semicolonToken = popToken();
            return addNode<ReturnStatement>(Tokens{ returnToken, semicolonToken });
        }
        Expression* value = parseExpression(0);
        Token semicolonToken = popExpectedToken(TokenType::SEMICOLON, "Expect ';' after return value.");
        return addNode<ReturnStatement>(Tokens{ returnToken, semicolonToken }, value);
    }

    // import "path" alias name; - 파일을 읽고(SourceReaderInterface) 재귀적으로
    // 다시 파싱해서(같은 tree를 공유하는 새 Parser) 그 파일의 최상위 선언들을
    // 뽑아낸다. Architecture.md §7.2 참고.
    ImportStatement* parseImport() {
        Token importToken = popToken();
        Token pathToken = popExpectedToken(TokenType::STRING, "Expect a file path string after 'import'.");
        popExpectedToken(TokenType::ALIAS, "Expect 'alias' after import path.");
        Token aliasToken = popExpectedToken(TokenType::IDENTIFIER, "Expect alias name.");
        Token semicolonToken = popExpectedToken(TokenType::SEMICOLON, "Expect ';' after import statement.");

        if (std::find(importStack.begin(), importStack.end(), pathToken.origin) != importStack.end()) {
            throw AssemblerError("순환 import: '{}'", pathToken.origin);
        }
        importStack.push_back(pathToken.origin);

        std::vector<Token> importedTokens;
        try {
            importedTokens = sourceReader.read(pathToken.origin);
        } catch (const std::exception& e) {
            importStack.pop_back();
            throw AssemblerError("import 대상 파일을 열 수 없습니다: '{}' ({})", pathToken.origin, e.what());
        }

        std::vector<Statement*> declarations;
        try {
            Parser importedParser(importedTokens, tree, operatorPriority, unaryOperator, sourceReader, importStack);
            for (Statement* decl : importedParser.parseProgram()) {
                if (!dynamic_cast<DeclareStatement*>(decl) && !dynamic_cast<FunctionDeclareStatement*>(decl)
                    && !dynamic_cast<ClassDeclareStatement*>(decl)) {
                    throw AssemblerError("import 대상 파일에는 선언 외의 내용을 허용하지 않습니다: '{}'", pathToken.origin);
                }
                declarations.push_back(decl);
            }
        } catch (...) {
            importStack.pop_back();
            throw;
        }
        importStack.pop_back();

        return addNode<ImportStatement>(Tokens{ importToken, semicolonToken }, aliasToken, declarations);
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

            // instanceof는 우변이 항상 클래스 이름(식별자) Token이어야 하므로
            // 다른 이항 연산자처럼 우변을 재귀적으로 Expression 파싱하지 않고
            // 별도로 처리한다.
            if (opToken.type == TokenType::INSTANCEOF) {
                Token className = popExpectedToken(TokenType::IDENTIFIER, "Expect class name after 'instanceof'.");
                left = addNode<InstanceOfExpression>(Tokens{ opToken }, left, className);
                continue;
            }

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
        return parseCall();
    }

    // primary()가 만든 표현식 뒤에 이어지는 "(", ".", "["을 반복적으로 처리해
    // 함수 호출/메서드 호출/필드 접근/인덱스 접근을 좌결합으로 파싱한다
    // (add(1,2), r.move(5), arr[i], r.list[0]() 같은 조합 모두 이 루프 하나로
    // 처리된다) - Architecture.md §3.1 참고.
    Expression* parseCall() {
        Expression* expr = parsePrimary();
        while (auto token = currentToken()) {
            if (token->type == TokenType::LEFT_PAREN) {
                Token leftParen = popToken();
                std::vector<Expression*> args;
                if (auto t = currentToken(); t && t->type != TokenType::RIGHT_PAREN) {
                    args.push_back(parseExpression(0));
                    for (auto comma = currentToken(); comma && comma->type == TokenType::COMMA; comma = currentToken()) {
                        popToken();
                        args.push_back(parseExpression(0));
                    }
                }
                Token rightParen = popExpectedToken(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");
                expr = addNode<CallExpression>(Tokens{ leftParen, rightParen }, expr, args);
            } else if (token->type == TokenType::DOT) {
                Token dot = popToken();
                Token name = popExpectedToken(TokenType::IDENTIFIER, "Expect property name after '.'.");
                expr = addNode<FieldAccessExpression>(Tokens{ dot }, expr, name);
            } else if (token->type == TokenType::LEFT_BRACKET) {
                Token leftBracket = popToken();
                Expression* index = parseExpression(0);
                Token rightBracket = popExpectedToken(TokenType::RIGHT_BRACKET, "Expect ']' after index.");
                expr = addNode<IndexExpression>(Tokens{ leftBracket, rightBracket }, expr, index);
            } else {
                break;
            }
        }
        return expr;
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
            case TokenType::THIS:
                popToken();
                return addNode<ThisExpression>(Tokens{ *token });
            case TokenType::SUPER:
                popToken();
                return addNode<SuperExpression>(Tokens{ *token });
            case TokenType::IDENTIFIER:
                popToken();
                return addNode<IdentifierExpression>(Tokens{ *token }, token->origin);
            case TokenType::ARRAY: {
                popToken();
                popExpectedToken(TokenType::LEFT_PAREN, "Expect '(' after 'Array'.");
                Expression* sizeExpr = parseExpression(0);
                popExpectedToken(TokenType::RIGHT_PAREN, "Expect ')' after array size.");
                return addNode<ArrayExpression>(Tokens{ *token }, sizeExpr);
            }
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
                // 대입 대상은 IdentifierExpression(a = 3), FieldAccessExpression
                // (r.speed = 3), IndexExpression(arr[i] = 3) 중 하나만 허용한다 -
                // Architecture.md §2.2 "AssignExpression 대상 일반화" 참고.
                if (!dynamic_cast<IdentifierExpression*>(left) && !dynamic_cast<FieldAccessExpression*>(left)
                    && !dynamic_cast<IndexExpression*>(left)) {
                    throw makeParseError("Invalid assignment target.", opToken);
                }
                return addNode<AssignExpression>(Tokens{ opToken }, left, right);
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
            case TokenType::SLASH: return addNode<DivideExpression>(Tokens{ opToken }, left, right);
            case TokenType::PERCENT: return addNode<ModExpression>(Tokens{ opToken }, left, right);
            case TokenType::AND: return addNode<AndExpression>(Tokens{ opToken }, left, right);
            case TokenType::OR: return addNode<OrExpression>(Tokens{ opToken }, left, right);
            default: throw AssemblerError("makeBinaryExpression: 처리되지 않은 연산자 토큰입니다.");
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
    SourceReaderInterface& sourceReader;
    std::vector<std::string>& importStack;
    size_t pos = 0;
};

}  // namespace

Assembler::Assembler(SourceReaderInterface& sourceReader) : sourceReader_(sourceReader) {
}

SyntaxTree Assembler::assemble(const Tokens& tokens) {
    SyntaxTree tree;
    Parser parser(tokens, tree, kDefaultOperatorPriority, kDefaultUnaryOperator, sourceReader_, importStack_);
    tree.setRoot(parser.parseStatement());
    return std::move(tree);
}
