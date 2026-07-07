#pragma once

#include <vector>

#include "Token.h"

// 문법 트리를 구성하는 노드의 공통 베이스.
// Statement / Expression 등 실제 노드 계층(PrintStatement, BinaryExpression 등)은
// Assembler / Checker / Executor 담당자가 이 클래스를 상속하여 구현한다.
// execute()/check() 의 세부 매개변수·반환 타입은 노드 종류별로 재정의될 수 있으므로
// 여기서는 SyntaxTree가 다형적으로 보관할 수 있는 최소 골격만 정의한다.
class SyntaxNode {
public:
    virtual ~SyntaxNode() = default;

    virtual void Execute() = 0;
    virtual bool Check() = 0;

    void AddChild(SyntaxNode* child) { childs_.push_back(child); }
    const std::vector<SyntaxNode*>& GetChilds() const { return childs_; }

protected:
    std::vector<SyntaxNode*> childs_;
    std::vector<Token> tokens_;
};
