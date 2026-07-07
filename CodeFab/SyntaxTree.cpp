#include "SyntaxTree.h"

#include <utility>

void SyntaxTree::add(std::unique_ptr<SyntaxNode> node) {
    if (nodes_.empty()) {
        root_ = node.get();
    }
    nodes_.push_back(std::move(node));
}
