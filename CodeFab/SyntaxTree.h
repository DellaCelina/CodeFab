#pragma once

#include <memory>
#include <vector>

#include "SyntaxNode.h"

// Assembler Unit이 조립한 문법 트리 조립체.
// 노드 소유권은 SyntaxTree가 갖고(unique_ptr), root는 진입점을 가리키는 비소유 포인터다.
// unique_ptr 멤버를 갖고 있으므로 복사는 불가능하고 이동만 가능하다.
class SyntaxTree {
public:
    SyntaxTree() = default;
    SyntaxTree(const SyntaxTree&) = delete;
    SyntaxTree& operator=(const SyntaxTree&) = delete;
    SyntaxTree(SyntaxTree&&) = default;
    SyntaxTree& operator=(SyntaxTree&&) = default;

    void add(std::unique_ptr<SyntaxNode> node);

    void setRoot(SyntaxNode* root) { root_ = root; }
    SyntaxNode* getRoot() const { return root_; }

private:
    std::vector<std::unique_ptr<SyntaxNode>> nodes_;
    SyntaxNode* root_ = nullptr;
};
